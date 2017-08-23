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

#include <ircd/ctx/continuation.h>
#include <ircd/socket.h>

namespace ircd {

ip::tcp::resolver *resolver;

} // namespace ircd

boost::asio::ssl::context
ircd::sslv23_client
{
	boost::asio::ssl::context::method::sslv23_client
};

ircd::socket::init::init()
{
	assert(ircd::ios);
	resolver = new ip::tcp::resolver(*ios);
}

ircd::socket::init::~init()
{
	delete resolver;
	resolver = nullptr;
}

size_t
ircd::write(socket &socket,
            const string_view &s)
{
	return write(socket, s.data(), s.size());
}

size_t
ircd::write(socket &socket,
            const char *const &buf,
            const size_t &size)
{
	const std::array<const_buffer, 1> bufs
	{{
		{ buf, size }
	}};

	return socket.write(bufs);
}

size_t
ircd::read(socket &socket,
           char *const &buf,
           const size_t &max)
{
	const std::array<mutable_buffer, 1> bufs
	{{
		{ buf, max }
	}};

	return socket.read_some(bufs);
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
		auto epit(resolver->async_resolve(query, yield(continuation())));
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
	sd.async_connect(ep, yield(continuation()));
	ssl.async_handshake(socket::handshake_type::client, yield(continuation()));
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
			ssl.async_shutdown(yield(continuation()));
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

//
// Overload for operator() without a timeout. see: operator()
//
void
ircd::socket::operator()(handler h)
{
	operator()(milliseconds(-1), std::move(h));
}

//
// This function calls back the handler when the socket has received
// something and is ready to be read from.
//
// The purpose here is to allow waiting for data from the socket without
// blocking any context and using any stack space whatsoever, i.e full
// asynchronous mode.
//
// boost::asio has no direct way to accomplish this, so we use a little
// trick to read a single byte with MSG_PEEK as our indication. This is
// done directly on the socket and not through the SSL cipher, but we
// don't want this byte anyway. This isn't such a great trick, because
// it may result in an extra syscall; so there's room for improvement here.
//
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
		throw;
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
