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
// net/net.h
//

namespace ircd::net
{
	ip::tcp::resolver *resolver;
}

ircd::net::init::init()
{
	net::resolver = new ip::tcp::resolver{*ircd::ios};
}

ircd::net::init::~init()
{
	assert(net::resolver);
	delete net::resolver;
	net::resolver = nullptr;
}

//
// net.h
//

std::string
ircd::net::string(const hostport &pair)
{
	std::string ret(256, char{});
	ret.resize(string(pair, mutable_buffer{ret}).size());
	return ret;
}

ircd::string_view
ircd::net::string(const hostport &pair,
                  const mutable_buffer &buf)
{
	const auto len
	{
		fmt::sprintf(buf, "%s:%u", pair.first, pair.second)
	};

	return { data(buf), size_t(len) };
}

size_t
ircd::net::read(socket &socket,
                iov<mutable_buffer> &bufs)
{
	const size_t read(socket.read_some(bufs));
	const size_t consumed(buffer::consume(bufs, read));
	assert(read == consumed);
	return read;
}

size_t
ircd::net::read(socket &socket,
                const iov<mutable_buffer> &bufs)
{
	return socket.read(bufs);
}

size_t
ircd::net::read(socket &socket,
                const mutable_buffer &buf)
{
	const ilist<mutable_buffer> bufs{buf};
	return socket.read(bufs);
}

size_t
ircd::net::write(socket &socket,
                 iov<const_buffer> &bufs)
{
	const size_t wrote(socket.write_some(bufs));
	const size_t consumed(consume(bufs, wrote));
	assert(wrote == consumed);
	return consumed;
}

size_t
ircd::net::write(socket &socket,
                 const iov<const_buffer> &bufs)
{
	const size_t wrote(socket.write(bufs));
	assert(wrote == size(bufs));
	return wrote;
}

size_t
ircd::net::write(socket &socket,
                 const const_buffer &buf)
{
	const ilist<const_buffer> bufs{buf};
	const size_t wrote(socket.write(bufs));
	assert(wrote == size(bufs));
	return wrote;
}

size_t
ircd::net::write(socket &socket,
                 const ilist<const_buffer> &bufs)
{
	const size_t wrote(socket.write(bufs));
	assert(wrote == size(bufs));
	return wrote;
}

///////////////////////////////////////////////////////////////////////////////
//
// net/listener.h
//

//
// ircd::net::listener
//

ircd::net::listener::listener(const std::string &opts)
:listener{json::object{opts}}
{
}

ircd::net::listener::listener(const json::object &opts)
:acceptor{std::make_unique<struct acceptor>(opts)}
{
}

ircd::net::listener::~listener()
noexcept
{
}

//
// ircd::net::listener::acceptor
//

struct ircd::net::listener::acceptor
{
	using error_code = boost::system::error_code;

	static log::log log;

	std::string name;
	size_t backlog;
	asio::ssl::context ssl;
	ip::tcp::endpoint ep;
	ip::tcp::acceptor a;

	explicit operator std::string() const;
	void configure(const json::object &opts);

	// Handshake stack
	bool handshake_error(const error_code &ec);
	void handshake(const error_code &ec, std::shared_ptr<socket>) noexcept;

	// Acceptance stack
	bool accept_error(const error_code &ec);
	void accept(const error_code &ec, std::shared_ptr<socket>) noexcept;

	// Accept next
	void next();

	acceptor(const json::object &opts);
	~acceptor() noexcept;
};

//
// ircd::net::listener::acceptor
//

ircd::log::log
ircd::net::listener::acceptor::log
{
	"listener"
};

ircd::net::listener::acceptor::acceptor(const json::object &opts)
try
:name
{
	unquote(opts.get("name", "IRCd (ssl)"s))
}
,backlog
{
	opts.get<size_t>("backlog", asio::socket_base::max_connections - 2)
}
,ssl
{
	asio::ssl::context::method::sslv23_server
}
,ep
{
	ip::address::from_string(unquote(opts.get("host", "127.0.0.1"s))),
	opts.get<uint16_t>("port", 6667)
}
,a
{
	*ircd::ios
}
{
	static const ip::tcp::acceptor::reuse_address reuse_address{true};

	configure(opts);

	log.debug("%s configured listener SSL",
	          std::string(*this));

	a.open(ep.protocol());
	a.set_option(reuse_address);
	log.debug("%s opened listener socket",
	          std::string(*this));

	a.bind(ep);
	log.debug("%s bound listener socket",
	          std::string(*this));

	a.listen(backlog);
	log.debug("%s listening (backlog: %lu)",
	          std::string(*this),
	          backlog);

	next();
}
catch(const boost::system::system_error &e)
{
	throw error("listener: %s", e.what());
}

ircd::net::listener::acceptor::~acceptor()
noexcept
{
	a.cancel();
}

void
ircd::net::listener::acceptor::next()
try
{
	auto sock(std::make_shared<ircd::socket>(ssl));
	log.debug("%s: listening with next socket(%p)",
	          std::string(*this),
	          sock.get());

	// The context blocks here until the next client is connected.
	auto accept(std::bind(&acceptor::accept, this, ph::_1, sock));
	a.async_accept(sock->sd, accept);
}
catch(const std::exception &e)
{
	log.critical("%s: %s",
	             std::string(*this),
	             e.what());

	if(ircd::debugmode)
		throw;
}

void
ircd::net::listener::acceptor::accept(const error_code &ec,
                                      const std::shared_ptr<socket> sock)
noexcept try
{
	if(accept_error(ec))
		return;

	log.debug("%s: accepted %s",
	          std::string(*this),
	          string(sock->remote()));

	//static const asio::socket_base::keep_alive keep_alive(true);
	//sock->sd.set_option(keep_alive);

	//static const asio::socket_base::linger linger(true, 30);
	//sock->sd.set_option(linger);

	//sock->sd.non_blocking(false);

	static const auto handshake_type
	{
		socket::handshake_type::server
	};

	auto handshake
	{
		std::bind(&acceptor::handshake, this, ph::_1, sock)
	};

	sock->ssl.async_handshake(handshake_type, std::move(handshake));
	next();
}
catch(const std::exception &e)
{
	log.error("%s: in accept(): socket(%p): %s",
	          std::string(*this),
	          sock.get(),
	          e.what());
	next();
}

bool
ircd::net::listener::acceptor::accept_error(const error_code &ec)
{
	switch(ec.value())
	{
		using namespace boost::system::errc;

		case success:
			return false;

		case operation_canceled:
			return true;

		default:
			throw boost::system::system_error(ec);
	}
}

void
ircd::net::listener::acceptor::handshake(const error_code &ec,
                                         const std::shared_ptr<socket> sock)
noexcept try
{
	if(handshake_error(ec))
		return;

	log.debug("%s SSL handshook %s",
	          std::string(*this),
	          string(sock->remote()));

	add_client(sock);
}
catch(const std::exception &e)
{
	log.error("%s: in handshake(): socket(%p)[%s]: %s",
	          std::string(*this),
	          sock.get(),
	          sock->connected()? string(sock->remote()) : "<gone>",
	          e.what());
}

bool
ircd::net::listener::acceptor::handshake_error(const error_code &ec)
{
	switch(ec.value())
	{
		using namespace boost::system::errc;

		case success:
			return false;

		case operation_canceled:
			return true;

		default:
			throw boost::system::system_error(ec);
	}
}

void
ircd::net::listener::acceptor::configure(const json::object &opts)
{
	log.debug("%s preparing listener socket configuration...",
	          std::string(*this));

	ssl.set_options
	(
		//ssl.default_workarounds
		//| ssl.no_tlsv1
		//| ssl.no_tlsv1_1
		//| ssl.no_tlsv1_2
		//| ssl.no_sslv2
		//| ssl.no_sslv3
		ssl.single_dh_use
	);


	//TODO: XXX
	ssl.set_password_callback([this]
	(const auto &size, const auto &purpose)
	{
		log.debug("%s asking for password with purpose '%s' (size: %zu)",
		          std::string(*this),
		          purpose,
		          size);

		//XXX: TODO
		return "foobar";
	});

	if(opts.has("ssl_certificate_chain_file"))
	{
		const std::string filename
		{
			unquote(opts["ssl_certificate_chain_file"])
		};

		ssl.use_certificate_chain_file(filename);
		log.info("%s using certificate chain file '%s'",
		          std::string(*this),
		          filename);
	}

	if(opts.has("ssl_certificate_file_pem"))
	{
		const std::string filename
		{
			unquote(opts["ssl_certificate_file_pem"])
		};

		ssl.use_certificate_file(filename, asio::ssl::context::pem);
		log.info("%s using certificate file '%s'",
		          std::string(*this),
		          filename);
	}

	if(opts.has("ssl_private_key_file_pem"))
	{
		const std::string filename
		{
			unquote(opts["ssl_private_key_file_pem"])
		};

		ssl.use_private_key_file(filename, asio::ssl::context::pem);
		log.info("%s using private key file '%s'",
		          std::string(*this),
		          filename);
	}

	if(opts.has("ssl_tmp_dh_file"))
	{
		const std::string filename
		{
			unquote(opts["ssl_tmp_dh_file"])
		};

		ssl.use_tmp_dh_file(filename);
		log.info("%s using tmp dh file '%s'",
		          std::string(*this),
		          filename);
	}
}

ircd::net::listener::acceptor::operator std::string()
const
{
	std::string ret(256, char{});
	const auto length
	{
		fmt::sprintf(mutable_buffer{ret}, "'%s' @ [%s]:%u",
		             name,
		             string(ep.address()),
		             ep.port())
	};

	ret.resize(length);
	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// net/socket.h
//

boost::asio::ssl::context
ircd::net::sslv23_client
{
	boost::asio::ssl::context::method::sslv23_client
};

size_t
ircd::net::available(const socket &s)
noexcept
{
	boost::system::error_code ec;
	return s.sd.available(ec);
}

ircd::net::hostport
ircd::net::local_hostport(const socket &socket)
{
    const auto &ep(socket.local());
    return { hostaddr(ep), port(ep) };
}

ircd::net::hostport
ircd::net::remote_hostport(const socket &socket)
{
    const auto &ep(socket.remote());
    return { hostaddr(ep), port(ep) };
}

bool
ircd::net::connected(const socket &s)
noexcept
{
	return s.connected();
}

uint16_t
ircd::net::port(const ip::tcp::endpoint &ep)
{
	return ep.port();
}

std::string
ircd::net::hostaddr(const ip::tcp::endpoint &ep)
{
	return string(address(ep));
}

std::string
ircd::net::string(const ip::address &addr)
{
	return addr.to_string();
}

std::string
ircd::net::string(const ip::tcp::endpoint &ep)
{
	std::string ret(256, char{});
	const auto addr{string(address(ep))};
	const auto data{const_cast<char *>(ret.data())};
	ret.resize(snprintf(data, ret.size(), "%s:%u", addr.c_str(), port(ep)));
	return ret;
}

boost::asio::ip::address
ircd::net::address(const ip::tcp::endpoint &ep)
{
	return ep.address();
}

//
// socket::io
//

ircd::net::socket::io::io(struct socket &sock,
                          struct stat &stat,
                          const std::function<size_t ()> &closure)
:sock{sock}
,stat{stat}
,bytes{closure()}
{
	stat.bytes += bytes;
	stat.calls++;
}

ircd::net::socket::io::operator size_t()
const
{
	return bytes;
}

//
// socket::scope_timeout
//

ircd::net::socket::scope_timeout::scope_timeout(socket &socket,
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

ircd::net::socket::scope_timeout::scope_timeout(socket &socket,
                                                const milliseconds &timeout,
                                                const socket::handler &handler)
:s{&socket}
{
	socket.set_timeout(timeout, handler);
}

ircd::net::socket::scope_timeout::~scope_timeout()
noexcept
{
	s->timer.cancel();
}

//
// socket
//

ircd::net::socket::socket(const std::string &host,
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

		log::debug("resolved remote %s:%u => %s",
		           host,
		           port,
		           string(*epit));

		return *epit;
	}(),
	timeout,
	ssl,
	ios
}
{
}

ircd::net::socket::socket(const ip::tcp::endpoint &remote,
                          const milliseconds &timeout,
                          asio::ssl::context &ssl,
                          boost::asio::io_service *const &ios)
try
:socket{ssl, ios}
{
	log::debug("socket(%p) attempting connect to remote: %s for the next %ld$ms",
	           this,
	           string(remote),
	           timeout.count());

	connect(remote, timeout);

	log::debug("socket(%p) connected to remote: %s from local: %s",
	           this,
	           string(remote),
	           string(local()));
}
catch(const std::exception &e)
{
	log::debug("socket(%p) failed to connect to remote %s: %s",
	           this,
	           string(remote),
	           e.what());
}

ircd::net::socket::socket(asio::ssl::context &ssl,
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

ircd::net::socket::~socket()
noexcept try
{
	disconnect(dc::RST);
}
catch(const boost::system::system_error &e)
{
	log::debug("socket(%p): close: %s", this, e.what());
	return;
}
catch(const std::exception &e)
{
	log::error("socket(%p): close: %s", this, e.what());
	return;
}

void
ircd::net::socket::connect(const ip::tcp::endpoint &ep,
                           const milliseconds &timeout)
{
	const scope_timeout ts(*this, timeout);
	sd.async_connect(ep, yield_context{to_asio{}});
	ssl.async_handshake(socket::handshake_type::client, yield_context{to_asio{}});
}

void
ircd::net::socket::disconnect(const dc &type)
{
	if(timer.expires_from_now() > 0ms)
		timer.cancel();

	if(sd.is_open())
		log::debug("socket(%p): disconnect: %s type: %d",
		           (const void *)this,
		           string(remote()),
		           uint(type));

	if(sd.is_open()) switch(type)
	{
		default:
		case dc::RST:
			sd.close();
			break;

		case dc::FIN:
			sd.shutdown(ip::tcp::socket::shutdown_both);
			break;

		case dc::FIN_SEND:
			sd.shutdown(ip::tcp::socket::shutdown_send);
			break;

		case dc::FIN_RECV:
			sd.shutdown(ip::tcp::socket::shutdown_receive);
			break;

		case dc::SSL_NOTIFY:
		{
			ssl.async_shutdown([s(shared_from_this())]
			(boost::system::error_code ec)
			{
				if(!ec)
					s->sd.close(ec);

				if(ec)
					log::warning("socket(%p): disconnect(): %s",
					             s.get(),
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

bool
ircd::net::socket::cancel()
noexcept
{
	boost::system::error_code ec[2];

	timer.cancel(ec[0]);
	sd.cancel(ec[1]);

	return std::all_of(begin(ec), end(ec), [](const auto &ec)
	{
		return ec == boost::system::errc::success;
	});
}

/// Asynchronous callback when the socket is ready
///
/// Overload for operator() without a timeout. see: operator()
///
void
ircd::net::socket::operator()(handler h)
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
ircd::net::socket::operator()(const milliseconds &timeout,
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
ircd::net::socket::handle(const std::weak_ptr<socket> wp,
                          const handler callback,
                          const error_code &ec,
                          const size_t &bytes)
noexcept try
{
	const life_guard<socket> s{wp};

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
catch(const std::bad_weak_ptr &e)
{
	// This handler may still be registered with asio after the socket destructs, so
	// the weak_ptr will indicate that fact. However, this is never intended and is
	// a debug assertion which should be corrected.
	log::warning("socket(%p): belated callback to handler... (%s)",
	             this,
	             e.what());
	assert(0);
}
catch(const std::exception &e)
{
	log::error("socket(%p): handle: %s",
	           this,
	           e.what());
	assert(0);
}

void
ircd::net::socket::call_user(const handler &callback,
                             const error_code &ec)
noexcept try
{
	callback(ec);
}
catch(const std::exception &e)
{
	log::error("socket(%p): async handler: unhandled exception: %s",
	           this,
	           e.what());
}

bool
ircd::net::socket::handle_error(const error_code &ec)
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
ircd::net::socket::handle_timeout(const std::weak_ptr<socket> wp,
                                  const error_code &ec)
noexcept try
{
 	using namespace boost::system::errc;

	if(!wp.expired()) switch(ec.value())
	{
		// A 'success' for this handler means there was a timeout on the socket
		case success:
			timedout = true;
			sd.cancel();
			break;

		// A cancelation means there was no timeout.
		case operation_canceled:
			timedout = false;
			break;

		// All other errors are unexpected, logged and ignored here.
		default:
			throw boost::system::system_error(ec);
	}
}
catch(const boost::system::system_error &e)
{
	log::error("socket(%p): handle_timeout: unexpected: %s\n",
	           (const void *)this,
			   e.what());
}
catch(const std::exception &e)
{
	log::error("socket(%p): handle timeout: %s",
	           (const void *)this,
	           e.what());
}

size_t
ircd::net::socket::available()
const
{
	return sd.available();
}

bool
ircd::net::socket::connected()
const noexcept try
{
	return sd.is_open();
}
catch(const boost::system::system_error &e)
{
	return false;
}

void
ircd::net::socket::set_timeout(const milliseconds &t)
{
	if(t < milliseconds(0))
		return;

	timer.expires_from_now(t);
	timer.async_wait(std::bind(&socket::handle_timeout, this, weak_from(*this), ph::_1));
}

void
ircd::net::socket::set_timeout(const milliseconds &t,
                          handler h)
{
	if(t < milliseconds(0))
		return;

	timer.expires_from_now(t);
	timer.async_wait(std::move(h));
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

ircd::buffer::mutable_buffer::operator
boost::asio::mutable_buffer()
const
{
	return boost::asio::mutable_buffer
	{
		data(*this), size(*this)
	};
}

ircd::buffer::const_buffer::operator
boost::asio::const_buffer()
const
{
	return boost::asio::const_buffer
	{
		data(*this), size(*this)
	};
}

ircd::buffer::mutable_raw_buffer::operator
boost::asio::mutable_buffer()
const
{
	return boost::asio::mutable_buffer
	{
		data(*this), size(*this)
	};
}

ircd::buffer::const_raw_buffer::operator
boost::asio::const_buffer()
const
{
	return boost::asio::const_buffer
	{
		data(*this), size(*this)
	};
}
