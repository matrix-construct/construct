/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <ircd/asio.h>

///////////////////////////////////////////////////////////////////////////////
//
// socket.h
//

boost::asio::ssl::context
ircd::sslv23_client
{
	boost::asio::ssl::context::method::sslv23_client
};

namespace ircd
{
	ip::tcp::resolver *resolver;
}

ircd::socket::init::init()
:resolver
{
	std::make_unique<ip::tcp::resolver>(*ios)
}
{
	ircd::resolver = resolver.get();
}

ircd::socket::init::~init()
{
	ircd::resolver = nullptr;
}

ircd::socket::scope_timeout::scope_timeout(socket &socket,
                                           const milliseconds &timeout)
:s{&socket}
{
    socket.set_timeout(timeout, [&socket]
    (const error_code &ec)
    {
        if(!ec)
            socket.sd.cancel();
    });
}

ircd::socket::scope_timeout::scope_timeout(socket &socket,
                                           const milliseconds &timeout,
                                           const socket::handler &handler)
:s{&socket}
{
	socket.set_timeout(timeout, handler);
}

ircd::socket::scope_timeout::~scope_timeout()
noexcept
{
	s->timer.cancel();
}

ircd::socket::socket(const std::string &host,
                     const uint16_t &port,
                     const milliseconds &timeout,
                     asio::ssl::context &ssl,
                     boost::asio::io_service *const &ios)
:socket
{
	[&host, &port]() -> ip::tcp::endpoint
	{
		assert(resolver);
		const ip::tcp::resolver::query query(host, string(lex_cast(port)));
		auto epit(resolver->async_resolve(query, yield_context{to_asio{}}));
		static const ip::tcp::resolver::iterator end;
		if(epit == end)
			throw nxdomain("host '%s' not found", host.data());

		return *epit;
	}(),
	timeout,
	ssl,
	ios
}
{
}

ircd::socket::socket(const ip::tcp::endpoint &remote,
                     const milliseconds &timeout,
                     asio::ssl::context &ssl,
                     boost::asio::io_service *const &ios)
:socket{ssl, ios}
{
	connect(remote, timeout);
}

ircd::socket::socket(asio::ssl::context &ssl,
                     boost::asio::io_service *const &ios)
:ssl
{
	*ios, ssl
}
,sd
{
	this->ssl.next_layer()
}
,timer
{
	*ios
}
,timedout
{
	false
}
{
}

ircd::socket::~socket()
noexcept
{
}

void
ircd::socket::connect(const ip::tcp::endpoint &ep,
                      const milliseconds &timeout)
{
	const scope_timeout ts(*this, timeout);
	sd.async_connect(ep, yield_context{to_asio{}});
	ssl.async_handshake(socket::handshake_type::client, yield_context{to_asio{}});
}

void
ircd::socket::disconnect(const dc &type)
{
	if(timer.expires_from_now() > 0ms)
		timer.cancel();

	if(sd.is_open()) switch(type)
	{
		default:
		case dc::RST:       sd.close();                                      break;
		case dc::FIN:       sd.shutdown(ip::tcp::socket::shutdown_both);     break;
		case dc::FIN_SEND:  sd.shutdown(ip::tcp::socket::shutdown_send);     break;
		case dc::FIN_RECV:  sd.shutdown(ip::tcp::socket::shutdown_receive);  break;

		case dc::SSL_NOTIFY:
		{
			ssl.async_shutdown([socket(shared_from_this())]
			(boost::system::error_code ec)
			{
				if(!ec)
					socket->sd.close(ec);

				if(ec)
					log::warning("socket(%p): disconnect(): %s",
					             socket.get(),
					             ec.message());
			});
			break;
		}

		case dc::SSL_NOTIFY_YIELD:
		{
			ssl.async_shutdown(yield_context{to_asio{}});
			sd.close();
			break;
		}
	}
}

void
ircd::socket::cancel()
{
	timer.cancel();
	sd.cancel();
}

/// Asynchronous callback when the socket is ready
///
/// Overload for operator() without a timeout. see: operator()
///
void
ircd::socket::operator()(handler h)
{
	operator()(milliseconds(-1), std::move(h));
}

/// Asynchronous callback when the socket is ready
///
/// This function calls back the handler when the socket has received
/// something and is ready to be read from.
///
/// The purpose here is to allow waiting for data from the socket without
/// blocking any context and using any stack space whatsoever, i.e full
/// asynchronous mode.
///
/// boost::asio has no direct way to accomplish this because the buffer size
/// must be positive so we use a little trick to read a single byte with
/// MSG_PEEK as our indication. This is done directly on the socket and
/// not through the SSL cipher, but we don't want this byte anyway. This
/// isn't such a great trick.
///
void
ircd::socket::operator()(const milliseconds &timeout,
                         handler callback)
{
	static const auto flags
	{
		ip::tcp::socket::message_peek
	};

	static char buffer[1];
	static const asio::mutable_buffers_1 buffers
	{
		buffer, sizeof(buffer)
	};

	auto handler
	{
		std::bind(&socket::handle, this, weak_from(*this), std::move(callback), ph::_1, ph::_2)
	};

	set_timeout(timeout);
	sd.async_receive(buffers, flags, std::move(handler));
}

void
ircd::socket::handle(const std::weak_ptr<socket> wp,
                     const handler callback,
                     const error_code &ec,
                     const size_t &bytes)
noexcept
{
	// This handler may still be registered with asio after the socket destructs, so
	// the weak_ptr will indicate that fact. However, this is never intended and is
	// a debug assertion which should be corrected.
	if(unlikely(wp.expired()))
	{
		log::warning("socket(%p): belated callback to handler...", this);
		assert(0);
		return;
	}

	// This handler and the timeout handler are responsible for canceling each other
	// when one or the other is entered. If the timeout handler has already fired for
	// a timeout on the socket, `timedout` will be `true` and this handler will be
	// entered with an `operation_canceled` error.
	if(!timedout)
		timer.cancel();
	else
		assert(ec == boost::system::errc::operation_canceled);

	// We can handle a few errors at this level which don't ever need to invoke the
	// user's callback. Otherwise they are passed up.
	if(!handle_error(ec))
	{
		log::debug("socket(%p): %s", this, ec.message());
		return;
	}

	call_user(callback, ec);
}

void
ircd::socket::call_user(const handler &callback,
                        const error_code &ec)
noexcept try
{
	callback(ec);
}
catch(const std::exception &e)
{
	log::error("socket(%p): async handler: unhandled user exception: %s",
	           this,
	           e.what());

	if(ircd::debugmode)
		std::terminate();
}

bool
ircd::socket::handle_error(const error_code &ec)
{
 	using namespace boost::system::errc;

	switch(ec.value())
	{
		// A success is not an error; can call the user handler
		case success:
			return true;

		// A cancel is triggered either by the timeout handler or by
		// a request to shutdown/close the socket. We only call the user's
		// handler for a timeout, otherwise this is hidden from the user.
		case operation_canceled:
			return timedout;

		// This indicates the remote closed the socket, we still
		// pass this up to the user so they can handle it.
		case boost::asio::error::eof:
			return true;

		// This is a condition which we hide from the user.
		case bad_file_descriptor:
			return false;

		// Everything else is passed up to the user.
		default:
			return true;
	}
}

void
ircd::socket::handle_timeout(const std::weak_ptr<socket> wp,
                             const error_code &ec)
{
 	using namespace boost::system::errc;

	if(!wp.expired()) switch(ec.value())
	{
		// A 'success' for this handler means there was a timeout on the socket
		case success:
			timedout = true;
			cancel();
			break;

		// A cancelation means there was no timeout.
		case operation_canceled:
			timedout = false;
			break;

		// All other errors are unexpected, logged and ignored here.
		default:
			log::error("socket::handle_timeout(): unexpected: %s\n",
			           ec.message());
			break;
	}
}

size_t
ircd::available(const socket &s)
{
	return s.sd.available();
}

bool
ircd::connected(const socket &s)
noexcept
{
	return s.connected();
}

uint16_t
ircd::port(const ip::tcp::endpoint &ep)
{
	return ep.port();
}

std::string
ircd::hostaddr(const ip::tcp::endpoint &ep)
{
	return string(address(ep));
}

std::string
ircd::string(const ip::address &addr)
{
	return addr.to_string();
}

boost::asio::ip::address
ircd::address(const ip::tcp::endpoint &ep)
{
	return ep.address();
}

size_t
ircd::read(socket &socket,
           iov<mutable_buffer> &bufs)
{
	const size_t read(socket.read_some(bufs));
	const size_t consumed(buffer::consume(bufs, read));
	assert(read == consumed);
	return read;
}

size_t
ircd::read(socket &socket,
           const iov<mutable_buffer> &bufs)
{
	return socket.read(bufs);
}

size_t
ircd::read(socket &socket,
           const mutable_buffer &buf)
{
	const ilist<mutable_buffer> bufs{buf};
	return socket.read(bufs);
}

size_t
ircd::write(socket &socket,
            iov<const_buffer> &bufs)
{
	const size_t wrote(socket.write_some(bufs));
	const size_t consumed(consume(bufs, wrote));
	assert(wrote == consumed);
	return consumed;
}

size_t
ircd::write(socket &socket,
            const iov<const_buffer> &bufs)
{
	const size_t wrote(socket.write(bufs));
	assert(wrote == size(bufs));
	return wrote;
}

size_t
ircd::write(socket &socket,
            const const_buffer &buf)
{
	const ilist<const_buffer> bufs{buf};
	const size_t wrote(socket.write(bufs));
	assert(wrote == size(bufs));
	return wrote;
}

size_t
ircd::write(socket &socket,
            const ilist<const_buffer> &bufs)
{
	const size_t wrote(socket.write(bufs));
	assert(wrote == size(bufs));
	return wrote;
}

bool
ircd::socket::connected()
const noexcept try
{
	return sd.is_open();
}
catch(const boost::system::system_error &e)
{
	return false;
}

void
ircd::socket::set_timeout(const milliseconds &t)
{
	if(t < milliseconds(0))
		return;

	timer.expires_from_now(t);
	timer.async_wait(std::bind(&socket::handle_timeout, this, weak_from(*this), ph::_1));
}

void
ircd::socket::set_timeout(const milliseconds &t,
                          handler h)
{
	if(t < milliseconds(0))
		return;

	timer.expires_from_now(t);
	timer.async_wait(std::move(h));
}

ircd::socket::io::io(struct socket &sock,
                     struct stat &stat,
                     const std::function<size_t ()> &closure)
:sock{sock}
,stat{stat}
,bytes{closure()}
{
	stat.bytes += bytes;
	stat.calls++;
}

ircd::socket::io::operator size_t()
const
{
	return bytes;
}

///////////////////////////////////////////////////////////////////////////////
//
// buffer.h - provide definition for the null buffers and asio conversion
//

const ircd::buffer::mutable_buffer
ircd::buffer::null_buffer
{
    nullptr, nullptr
};

const ircd::ilist<ircd::buffer::mutable_buffer>
ircd::buffer::null_buffers
{{
    null_buffer
}};

ircd::buffer::mutable_buffer::operator boost::asio::mutable_buffer()
const
{
	return boost::asio::mutable_buffer
	{
		data(*this), size(*this)
	};
}

ircd::buffer::const_buffer::operator boost::asio::const_buffer()
const
{
	return boost::asio::const_buffer
	{
		data(*this), size(*this)
	};
}
