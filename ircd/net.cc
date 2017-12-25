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

struct ircd::log::log
ircd::net::log
{
	"net", 'N'
};

/// Network subsystem initialization
///
ircd::net::init::init()
{
	net::resolver = new ip::tcp::resolver{*ircd::ios};
}

/// Network subsystem shutdown
///
ircd::net::init::~init()
{
	assert(net::resolver);
	delete net::resolver;
	net::resolver = nullptr;
}

//
// socket (public)
//

ircd::const_raw_buffer
ircd::net::peer_cert_der(const mutable_raw_buffer &buf,
                         const socket &socket)
{
	const SSL &ssl(socket);
	const X509 &cert(openssl::get_peer_cert(ssl));
	return openssl::i2d(buf, cert);
}

std::shared_ptr<ircd::net::socket>
ircd::net::connect(const net::remote &remote,
                   const milliseconds &timeout)
{
	const asio::ip::tcp::endpoint ep
	{
		is_v6(remote)? asio::ip::tcp::endpoint
		{
			asio::ip::address_v6 { std::get<remote.IP>(remote) }, port(remote)
		}
		: asio::ip::tcp::endpoint
		{
			asio::ip::address_v4 { host4(remote) }, port(remote)
		},
	};

	return connect(ep, timeout);
}

std::shared_ptr<ircd::net::socket>
ircd::net::connect(const ip::tcp::endpoint &remote,
                   const milliseconds &timeout)
{
	const auto ret(std::make_shared<socket>());
	ret->connect(remote, timeout);
	return ret;
}

bool
ircd::net::disconnect(socket &socket,
                      const dc &type)
noexcept try
{
	socket.disconnect(type);
	return true;
}
catch(const std::exception &e)
{
/*
	log::error("socket(%p): disconnect: type: %d: %s",
	           this,
	           int(type),
	           e.what());
*/
	return false;
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

struct ircd::net::listener::acceptor
:std::enable_shared_from_this<struct ircd::net::listener::acceptor>
{
	using error_code = boost::system::error_code;

	static log::log log;

	std::string name;
	size_t backlog;
	asio::ssl::context ssl;
	ip::tcp::endpoint ep;
	ip::tcp::acceptor a;
	size_t accepting {0};
	size_t handshaking {0};
	bool interrupting {false};
	ctx::dock joining;

	explicit operator std::string() const;
	void configure(const json::object &opts);

	// Handshake stack
	bool handshake_error(const error_code &ec, socket &);
	void handshake(const error_code &ec, std::shared_ptr<socket>, std::weak_ptr<acceptor>) noexcept;

	// Acceptance stack
	bool accept_error(const error_code &ec, socket &);
	void accept(const error_code &ec, std::shared_ptr<socket>, std::weak_ptr<acceptor>) noexcept;

	// Accept next
	void next();

	// Acceptor shutdown
	bool interrupt() noexcept;
	void join() noexcept;

	acceptor(const json::object &opts);
	~acceptor() noexcept;
};

//
// ircd::net::listener
//

ircd::net::listener::listener(const std::string &opts)
:listener{json::object{opts}}
{
}

ircd::net::listener::listener(const json::object &opts)
:acceptor{std::make_shared<struct acceptor>(opts)}
{
	// Starts the first asynchronous accept. This has to be done out here after
	// the acceptor's shared object is constructed.
	acceptor->next();
}

/// Cancels all pending accepts and handshakes and waits (yields ircd::ctx)
/// until report.
///
ircd::net::listener::~listener()
noexcept
{
	if(acceptor)
		acceptor->join();
}

void
ircd::net::listener::acceptor::join()
noexcept try
{
	interrupt();
	joining.wait([this]
	{
		return !accepting && !handshaking;
	});
}
catch(const std::exception &e)
{
	log.error("acceptor(%p): join: %s",
	          this,
	          e.what());
}

bool
ircd::net::listener::acceptor::interrupt()
noexcept try
{
	a.cancel();
	interrupting = true;
	return true;
}
catch(const boost::system::system_error &e)
{
	log.error("acceptor(%p): interrupt: %s",
	          this,
	          string(e));

	return false;
}

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
	//boost::asio::ip::tcp::socket::max_connections   <-- linkage failed?
	opts.get<size_t>("backlog", SOMAXCONN) //TODO: XXX
}
,ssl
{
	asio::ssl::context::method::sslv23_server
}
,ep
{
	ip::address::from_string(unquote(opts.get("host", "127.0.0.1"s))),
	opts.at<uint16_t>("port")
}
,a
{
	*ircd::ios
}
{
	static const auto &max_connections
	{
		//boost::asio::ip::tcp::socket::max_connections   <-- linkage failed?
		SOMAXCONN //TODO: XXX
	};

	static const ip::tcp::acceptor::reuse_address reuse_address
	{
		true
	};

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
	log.debug("%s listening (backlog: %lu, max connections: %zu)",
	          std::string(*this),
	          backlog,
	          max_connections);
}
catch(const boost::system::system_error &e)
{
	throw error("listener: %s", e.what());
}

ircd::net::listener::acceptor::~acceptor()
noexcept
{
}

/// Sets the next asynchronous handler to start the next accept sequence.
/// Each call to next() sets one handler which handles the connect for one
/// socket. After the connect, an asynchronous SSL handshake handler is set
/// for the socket, and next() is called again to setup for the next socket
/// too.
void
ircd::net::listener::acceptor::next()
try
{
	auto sock(std::make_shared<ircd::socket>(ssl));
/*
	log.debug("%s: socket(%p) is the next socket to accept",
	          std::string(*this),
	          sock.get());
*/
	ip::tcp::socket &sd(*sock);
	a.async_accept(sd, std::bind(&acceptor::accept, this, ph::_1, sock, weak_from(*this)));
	++accepting;
}
catch(const std::exception &e)
{
	log.critical("%s: %s",
	             std::string(*this),
	             e.what());

	if(ircd::debugmode)
		throw;
}

/// Callback for a socket connected. This handler then invokes the
/// asynchronous SSL handshake sequence.
///
void
ircd::net::listener::acceptor::accept(const error_code &ec,
                                      const std::shared_ptr<socket> sock,
                                      const std::weak_ptr<acceptor> a)
noexcept try
{
	if(unlikely(a.expired()))
		return;

	--accepting;
	const unwind::nominal next{[this]
	{
		this->next();
	}};

	const unwind::exceptional drop{[&sock]
	{
		assert(bool(sock));
		disconnect(*sock, dc::RST);
	}};

	assert(bool(sock));
	if(unlikely(accept_error(ec, *sock)))
	{
		disconnect(*sock, dc::RST);
		return;
	}

	log.debug("%s: socket(%p) accepted %s",
	          std::string(*this),
	          sock.get(),
	          string(sock->remote()));

	static const socket::handshake_type handshake_type
	{
		socket::handshake_type::server
	};

	auto handshake
	{
		std::bind(&acceptor::handshake, this, ph::_1, sock, a)
	};

	sock->ssl.async_handshake(handshake_type, std::move(handshake));
	++handshaking;
}
catch(const ctx::interrupted &e)
{
	log.debug("%s: acceptor interrupted socket(%p): %s",
	          std::string(*this),
	          sock.get(),
	          string(ec));

	joining.notify_all();
}
catch(const std::exception &e)
{
	log.error("%s: socket(%p): in accept(): [%s]: %s",
	          std::string(*this),
	          sock.get(),
	          sock->connected()? string(sock->remote()) : "<gone>",
	          e.what());
}

/// Error handler for the accept socket callback. This handler determines
/// whether or not the handler should return or continue processing the
/// result.
///
bool
ircd::net::listener::acceptor::accept_error(const error_code &ec,
                                            socket &sock)
{
	using namespace boost::system::errc;
	using boost::system::get_system_category;

	if(unlikely(interrupting))
		throw ctx::interrupted();

	if(likely(ec == success))
		return false;

	if(ec.category() == get_system_category()) switch(ec.value())
	{
		case operation_canceled:
			return false;

		default:
			break;
	}

	throw boost::system::system_error(ec);
}

void
ircd::net::listener::acceptor::handshake(const error_code &ec,
                                         const std::shared_ptr<socket> sock,
                                         const std::weak_ptr<acceptor> a)
noexcept try
{
	if(unlikely(a.expired()))
		return;

	--handshaking;
	assert(bool(sock));
	const unwind::exceptional drop{[&sock]
	{
		disconnect(*sock, dc::RST);
	}};

	if(unlikely(handshake_error(ec, *sock)))
	{
		disconnect(*sock, dc::RST);
		return;
	}

	log.debug("%s socket(%p): SSL handshook %s",
	          std::string(*this),
	          sock.get(),
	          string(sock->remote()));

	add_client(sock);
}
catch(const ctx::interrupted &e)
{
	log.debug("%s: SSL handshake interrupted socket(%p): %s",
	          std::string(*this),
	          sock.get(),
	          string(ec));

	joining.notify_all();
}
catch(const std::exception &e)
{
	log.error("%s: socket(%p): in handshake(): [%s]: %s",
	          std::string(*this),
	          sock.get(),
	          sock->connected()? string(sock->remote()) : "<gone>",
	          e.what());
}

/// Error handler for the SSL handshake callback. This handler determines
/// whether or not the handler should return or continue processing the
/// result.
///
bool
ircd::net::listener::acceptor::handshake_error(const error_code &ec,
                                               socket &sock)
{
	using boost::system::get_system_category;
	using namespace boost::system::errc;

	if(unlikely(interrupting))
		throw ctx::interrupted();

	if(likely(ec == success))
		return false;

	if(ec.category() == get_system_category()) switch(ec.value())
	{
		case operation_canceled:
			return false;

		default:
			break;
	}

	throw boost::system::system_error(ec);
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

		if(!fs::exists(filename))
			throw error("%s: SSL certificate chain file @ `%s' not found",
			            std::string(*this),
			            filename);

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

		if(!fs::exists(filename))
			throw error("%s: SSL certificate pem file @ `%s' not found",
			            std::string(*this),
			            filename);

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

		if(!fs::exists(filename))
			throw error("%s: SSL private key file @ `%s' not found",
			            std::string(*this),
			            filename);

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

		if(!fs::exists(filename))
			throw error("%s: SSL tmp dh file @ `%s' not found",
			            std::string(*this),
			            filename);

		ssl.use_tmp_dh_file(filename);
		log.info("%s using tmp dh file '%s'",
		         std::string(*this),
		         filename);
	}
}

ircd::net::listener::acceptor::operator std::string()
const
{
	return fmt::snstringf
	{
		256, "'%s' @ [%s]:%u", name, string(ep.address()), ep.port()
	};
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

ircd::net::hostport
ircd::net::local_hostport(const socket &socket)
noexcept try
{
    const auto &ep(socket.local());
    return { host(ep), port(ep) };
}
catch(...)
{
	return { std::string{}, 0 };
}

ircd::net::hostport
ircd::net::remote_hostport(const socket &socket)
noexcept try
{
    const auto &ep(socket.remote());
    return { host(ep), port(ep) };
}
catch(...)
{
	return { std::string{}, 0 };
}

ircd::net::ipport
ircd::net::local_ipport(const socket &socket)
noexcept try
{
    const auto &ep(socket.local());
    const auto &a(addr(ep));

	ipport ret;
	if(a.is_v6())
	{
		std::get<ret.IP>(ret) = a.to_v6().to_bytes();
		std::reverse(std::get<ret.IP>(ret).begin(), std::get<ret.IP>(ret).end());
	}
	else host4(ret) = a.to_v4().to_ulong();

	return ret;
}
catch(...)
{
	return {};
}

ircd::net::ipport
ircd::net::remote_ipport(const socket &socket)
noexcept try
{
    const auto &ep(socket.remote());
    const auto &a(addr(ep));

	ipport ret;
	if(a.is_v6())
	{
		std::get<ret.IP>(ret) = a.to_v6().to_bytes();
		std::reverse(std::get<ret.IP>(ret).begin(), std::get<ret.IP>(ret).end());
	}
	else host4(ret) = a.to_v4().to_ulong();

	port(ret) = port(ep);
	return ret;
}
catch(...)
{
	return {};
}

size_t
ircd::net::available(const socket &s)
noexcept
{
	boost::system::error_code ec;
	const ip::tcp::socket &sd(s);
	return sd.available(ec);
}

bool
ircd::net::connected(const socket &s)
noexcept
{
	return s.connected();
}

//
// socket::io
//

ircd::net::socket::io::io(struct socket &sock,
                          struct stat &stat,
                          const std::function<size_t ()> &closure)
:io
{
	sock, stat, closure()
}
{}

ircd::net::socket::io::io(struct socket &sock,
                          struct stat &stat,
                          const size_t &bytes)
:sock{sock}
,stat{stat}
,bytes{bytes}
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
	socket.set_timeout(timeout);
}

ircd::net::socket::scope_timeout::scope_timeout(socket &socket,
                                                const milliseconds &timeout,
                                                socket::handler handler)
:s{&socket}
{
	socket.set_timeout(timeout, std::move(handler));
}

ircd::net::socket::scope_timeout::scope_timeout(scope_timeout &&other)
noexcept
:s{std::move(other.s)}
{
	other.s = nullptr;
}

ircd::net::socket::scope_timeout &
ircd::net::socket::scope_timeout::operator=(scope_timeout &&other)
noexcept
{
	this->~scope_timeout();
	s = std::move(other.s);
	return *this;
}

ircd::net::socket::scope_timeout::~scope_timeout()
noexcept
{
	cancel();
}

bool
ircd::net::socket::scope_timeout::cancel()
noexcept try
{
	if(!this->s)
		return false;

	auto *const s{this->s};
	this->s = nullptr;
	s->cancel_timeout();
	return true;
}
catch(const std::exception &e)
{
	log.error("socket(%p) scope_timeout::cancel: %s",
	          (const void *)s,
	          e.what());

	return false;
}

bool
ircd::net::socket::scope_timeout::release()
{
	const auto s{this->s};
	this->s = nullptr;
	return s != nullptr;
}

//
// socket
//

ircd::net::socket::socket(asio::ssl::context &ssl,
                          boost::asio::io_service *const &ios)
:sd
{
	*ios
}
,ssl
{
	this->sd, ssl
}
,timer
{
	*ios
}
{
}

/// The dtor asserts that the socket is not open/connected requiring a
/// an SSL close_notify. There's no more room for async callbacks via
/// shared_ptr after this dtor.
ircd::net::socket::~socket()
noexcept try
{
	if(unlikely(RB_DEBUG_LEVEL && connected()))
		log.critical("Failed to ensure socket(%p) is disconnected from %s before dtor.",
		             this,
		             string(remote()));

	assert(!connected());
}
catch(const std::exception &e)
{
	log.critical("socket(%p): close: %s", this, e.what());
	return;
}

/// Attempt to connect and ssl handshake remote; yields ircd::ctx; throws timeout
///
void
ircd::net::socket::connect(const ip::tcp::endpoint &ep,
                           const milliseconds &timeout)
try
{
	const life_guard<socket> lg{*this};
	const scope_timeout ts{*this, timeout};
	log.debug("socket(%p) attempting connect to remote: %s for the next %ld$ms",
	          this,
	          string(ep),
	          timeout.count());

	sd.async_connect(ep, yield_context{to_asio{}});
	log.debug("socket(%p) connected to remote: %s from local: %s; performing handshake...",
	          this,
	          string(ep),
	          string(local()));

	ssl.async_handshake(socket::handshake_type::client, yield_context{to_asio{}});
	log.debug("socket(%p) secure session with %s from local: %s established.",
	          this,
	          string(ep),
	          string(local()));
}
catch(const std::exception &e)
{
	log.debug("socket(%p) failed to connect to remote %s: %s",
	          this,
	          string(ep),
	          e.what());

	disconnect(dc::RST);
	throw;
}

/// Attempt to connect and ssl handshake remote; yields ircd::ctx; throws timeout
///
void
ircd::net::socket::connect(const net::remote &remote,
                           const milliseconds &timeout)
{
	const ip::tcp::endpoint ep
	{
		is_v6(remote)? asio::ip::tcp::endpoint
		{
			asio::ip::address_v6 { std::get<remote.IP>(remote) }, port(remote)
		}
		: asio::ip::tcp::endpoint
		{
			asio::ip::address_v4 { host4(remote) }, port(remote)
		},
	};

	this->connect(ep, timeout);
}

/// Attempt to connect and ssl handshake; asynchronous, callback when done.
///
void
ircd::net::socket::connect(const ip::tcp::endpoint &ep,
                           const milliseconds &timeout,
                           handler callback)
{
	auto handshake_handler{[this, callback(std::move(callback))]
	(const error_code &ec)
	noexcept
	{
		if(timedout)
			assert(ec == boost::system::errc::operation_canceled);

		if(!timedout)
			cancel_timeout();

		try
		{
			callback(ec);
		}
		catch(const std::exception &e)
		{
			log.critical("socket(%p): connect: unhandled exception from user callback: %s",
			             (const void *)this,
			             e.what());
		}
	}};

	auto connect_handler{[this, handshake_handler(std::move(handshake_handler))]
	(const error_code &ec)
	noexcept
	{
		// Even though the branch on ec below should cancel the timeout on
		// error, the timeout still needs to be canceled if else anything bad
		// happens in the remainder of this frame too.
		const unwind::exceptional cancels{[this]
		{
			cancel_timeout();
		}};

		// A connect error
		if(ec)
		{
			handshake_handler(ec);
			return;
		}

		static const auto handshake{socket::handshake_type::client};
		ssl.async_handshake(handshake, std::move(handshake_handler));
	}};

	set_timeout(timeout);
	sd.async_connect(ep, std::move(connect_handler));
}

bool
ircd::net::socket::disconnect(const dc &type)
try
{
	if(timer.expires_from_now() > 0ms)
		timer.cancel();

	if(sd.is_open())
		log.debug("socket(%p): disconnect: %s type:%d user: in:%zu out:%zu",
		          (const void *)this,
		          string(remote_ipport(*this)),
		          uint(type),
		          in.bytes,
		          out.bytes);

	if(sd.is_open()) switch(type)
	{
		default:
		case dc::RST:
			sd.close();
			return true;

		case dc::FIN:
			sd.shutdown(ip::tcp::socket::shutdown_both);
			return true;

		case dc::FIN_SEND:
			sd.shutdown(ip::tcp::socket::shutdown_send);
			return true;

		case dc::FIN_RECV:
			sd.shutdown(ip::tcp::socket::shutdown_receive);
			return true;

		case dc::SSL_NOTIFY_YIELD: if(likely(ctx::current))
		{
			const life_guard<socket> lg{*this};
			const scope_timeout ts{*this, 8s};
			ssl.async_shutdown(yield_context{to_asio{}});
			error_code ec;
			sd.close(ec);
			if(ec)
				log.error("socket(%p): close: %s: %s",
				          this,
				          string(ec));
			return true;
		}

		case dc::SSL_NOTIFY:
		{
			set_timeout(8s);
			ssl.async_shutdown([s(shared_from_this())]
			(error_code ec)
			noexcept
			{
				if(!s->timedout)
					s->cancel_timeout();

				if(ec)
					log.warning("socket(%p): SSL_NOTIFY: %s: %s",
					            s.get(),
					            string(ec));

				if(!s->sd.is_open())
					return;

				s->sd.close(ec);

				if(ec)
					log.warning("socket(%p): after SSL_NOTIFY: %s: %s",
					            s.get(),
					            string(ec));
			});
			return true;
		}
	}
	else return false;
}
catch(const boost::system::system_error &e)
{
	log.warning("socket(%p): disconnect: type: %d: %s",
	            (const void *)this,
	            uint(type),
	            e.what());

	if(sd.is_open())
	{
		boost::system::error_code ec;
		sd.close(ec);
		if(ec)
			log.warning("socket(%p): after disconnect: %s: %s",
			            this,
			            string(ec));
	}

	throw;
}

bool
ircd::net::socket::cancel()
noexcept
{
	boost::system::error_code ec[2];
	sd.cancel(ec[0]);
	timer.cancel(ec[1]);

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
void
ircd::net::socket::operator()(const milliseconds &timeout,
                              handler callback)
{
	static const auto flags
	{
		ip::tcp::socket::message_peek
	};

	static char buffer[0];
	static const asio::mutable_buffers_1 buffers
	{
		buffer, sizeof(buffer)
	};

	auto handler
	{
		std::bind(&socket::handle, this, weak_from(*this), std::move(callback), ph::_1, ph::_2)
	};

	assert(connected());
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
	// After life_guard is constructed it is safe to use *this in this frame.
	const life_guard<socket> s{wp};

/*
	log.debug("socket(%p): %zu bytes; %s: %s",
	          this,
	          bytes,
	          string(ec));
*/

	// This handler and the timeout handler are responsible for canceling each other
	// when one or the other is entered. If the timeout handler has already fired for
	// a timeout on the socket, `timedout` will be `true` and this handler will be
	// entered with an `operation_canceled` error.
	if(!timedout)
		cancel_timeout();
	else
		assert(ec == boost::system::errc::operation_canceled);

	// We can handle a few errors at this level which don't ever need to invoke the
	// user's callback. Otherwise they are passed up.
	if(!handle_error(ec))
	{
		log.error("socket(%p): %s",
		          this,
		          string(ec));
		return;
	}

	call_user(callback, ec);
}
catch(const std::bad_weak_ptr &e)
{
	// This handler may still be registered with asio after the socket destructs, so
	// the weak_ptr will indicate that fact. However, this is never intended and is
	// a debug assertion which should be corrected.
	log.warning("socket(%p): belated callback to handler... (%s)",
	            this,
	            e.what());
	assert(0);
}
catch(const boost::system::system_error &e)
{
	log.error("socket(%p): handle: %s %s",
	          this,
	          string(ec));
	assert(0);
}
catch(const std::exception &e)
{
	log.error("socket(%p): handle: %s",
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
	log.critical("socket(%p): async handler: unhandled exception: %s",
	             this,
	             e.what());
}

bool
ircd::net::socket::handle_error(const error_code &ec)
{
 	using namespace boost::system::errc;
	using boost::system::get_system_category;
	using boost::asio::error::get_ssl_category;
	using boost::asio::error::get_misc_category;

	if(likely(ec == success))
		return true;

	log.warning("socket(%p): handle error: %s: %s",
	            this,
	            string(ec));

	if(ec.category() == get_system_category()) switch(ec.value())
	{
		// A cancel is triggered either by the timeout handler or by
		// a request to shutdown/close the socket. We only call the user's
		// handler for a timeout, otherwise this is hidden from the user.
		case operation_canceled:
			return timedout;

		// This is a condition which we hide from the user.
		case bad_file_descriptor:
			return false;

		// Everything else is passed up to the user.
		default:
			return true;
	}
	else if(ec.category() == get_ssl_category()) switch(uint8_t(ec.value()))
	{
		// Docs say this means we read less bytes off the socket than desired.
		case SSL_R_SHORT_READ:
			return true;

		default:
			return true;
	}
	else if(ec.category() == get_misc_category()) switch(ec.value())
	{
		// This indicates the remote closed the socket, we still
		// pass this up to the user so they can know that too.
		case boost::asio::error::eof:
			return true;

		default:
			return true;
	}

	assert(0);
	return true;
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
		{
			sd.cancel();
			assert(timedout == false);
			timedout = true;
			break;
		}

		// A cancelation means there was no timeout.
		case operation_canceled:
		{
			assert(ec.category() == boost::system::get_system_category());
			assert(timedout == false);
			timedout = false;
			break;
		}

		// All other errors are unexpected, logged and ignored here.
		default:
			throw boost::system::system_error(ec);
	}
}
catch(const boost::system::system_error &e)
{
	log.error("socket(%p): handle_timeout: unexpected: %s\n",
	          (const void *)this,
	          e.what());
}
catch(const std::exception &e)
{
	log.error("socket(%p): handle timeout: %s",
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

ircd::net::error_code
ircd::net::socket::cancel_timeout()
noexcept
{
	boost::system::error_code ec;
	timedout = false;
	timer.cancel(ec);
	return ec;
}

boost::asio::ip::tcp::endpoint
ircd::net::socket::local()
const
{
	return sd.local_endpoint();
}

boost::asio::ip::tcp::endpoint
ircd::net::socket::remote()
const
{
	return sd.remote_endpoint();
}

void
ircd::net::socket::set_timeout(const milliseconds &t)
{
	cancel_timeout();
	if(t < milliseconds(0))
		return;

	timer.expires_from_now(t);
	timer.async_wait(std::bind(&socket::handle_timeout, this, weak_from(*this), ph::_1));
}

void
ircd::net::socket::set_timeout(const milliseconds &t,
                               handler h)
{
	cancel_timeout();
	if(t < milliseconds(0))
		return;

	timer.expires_from_now(t);
	timer.async_wait(std::move(h));
}

ircd::net::socket::operator
SSL &()
{
	assert(ssl.native_handle());
	return *ssl.native_handle();
}

ircd::net::socket::operator
const SSL &()
const
{
	using type = typename std::remove_const<decltype(socket::ssl)>::type;
	auto &ssl(const_cast<type &>(this->ssl));
	assert(ssl.native_handle());
	return *ssl.native_handle();
}

///////////////////////////////////////////////////////////////////////////////
//
// net/asio.h
//

std::string
ircd::net::string(const ip::address &addr)
{
	return addr.to_string();
}

std::string
ircd::net::string(const ip::tcp::endpoint &ep)
{
	std::string ret(256, char{});
	const auto addr{string(net::addr(ep))};
	const auto data{const_cast<char *>(ret.data())};
	ret.resize(snprintf(data, ret.size(), "%s:%u", addr.c_str(), port(ep)));
	return ret;
}

std::string
ircd::net::host(const ip::tcp::endpoint &ep)
{
	return string(addr(ep));
}

boost::asio::ip::address
ircd::net::addr(const ip::tcp::endpoint &ep)
{
	return ep.address();
}

uint16_t
ircd::net::port(const ip::tcp::endpoint &ep)
{
	return ep.port();
}

std::string
ircd::net::string(const boost::system::system_error &e)
{
	return string(e.code());
}

std::string
ircd::net::string(const boost::system::error_code &ec)
{
	std::string ret(128, char{});
	ret.resize(string(mutable_buffer{ret}, ec).size());
	return ret;
}

ircd::string_view
ircd::net::string(const mutable_buffer &buf,
                  const boost::system::system_error &e)
{
	return string(buf, e.code());
}

ircd::string_view
ircd::net::string(const mutable_buffer &buf,
                  const boost::system::error_code &ec)
{
	const auto len
	{
		fmt::sprintf
		{
			buf, "%s: %s", ec.category().name(), ec.message()
		}
	};

	return { data(buf), size_t(len) };
}

///////////////////////////////////////////////////////////////////////////////
//
// net/remote.h
//

const ircd::net::remote
ircd::net::null_remote
{};

//
// host / port utils
//

std::ostream &
ircd::net::operator<<(std::ostream &s, const hostport &t)
{
	char buf[256];
	s << string(t, buf);
	return s;
}

std::ostream &
ircd::net::operator<<(std::ostream &s, const ipport &t)
{
	char buf[256];
	s << string(t, buf);
	return s;
}

std::ostream &
ircd::net::operator<<(std::ostream &s, const remote &t)
{
	char buf[256];
	s << string(t, buf);
	return s;
}

namespace ircd::net
{
	template<class T> std::string _string(const T &t);
}

template<class T>
std::string
ircd::net::_string(const T &t)
{
	std::string ret(256, char{});
	ret.resize(net::string(t, mutable_buffer{ret}).size());
	return ret;
}

std::string
ircd::net::string(const uint32_t &t)
{
	return _string(t);
}

std::string
ircd::net::string(const uint128_t &t)
{
	return _string(t);
}

std::string
ircd::net::string(const hostport &t)
{
	return _string(t);
}

std::string
ircd::net::string(const ipport &t)
{
	return _string(t);
}

std::string
ircd::net::string(const remote &t)
{
	return _string(t);
}

ircd::string_view
ircd::net::string(const uint32_t &ip,
                  const mutable_buffer &buf)
{
	const auto len
	{
		fmt::sprintf(buf, "%s", ip::address_v4{ip}.to_string())
	};

	return { data(buf), size_t(len) };
}

ircd::string_view
ircd::net::string(const uint128_t &ip,
                  const mutable_buffer &buf)
{
	const auto &pun
	{
		reinterpret_cast<const uint8_t (&)[16]>(ip)
	};

	const auto &punpun
	{
		reinterpret_cast<const std::array<uint8_t, 16> &>(pun)
	};

	const auto len
	{
		fmt::sprintf(buf, "%s", ip::address_v6{punpun}.to_string())
	};

	return { data(buf), size_t(len) };
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

ircd::string_view
ircd::net::string(const ipport &ipp,
                  const mutable_buffer &buf)
{
	const auto len
	{
		is_v4(ipp)?
		fmt::sprintf(buf, "%s:%u",
		             ip::address_v4{host4(ipp)}.to_string(),
		             port(ipp)):

		is_v6(ipp)?
		fmt::sprintf(buf, "%s:%u",
		             ip::address_v6{std::get<ipp.IP>(ipp)}.to_string(),
		             port(ipp)):

		0
	};

	return { data(buf), size_t(len) };
}

ircd::string_view
ircd::net::string(const remote &remote,
                  const mutable_buffer &buf)
{
	const auto &ipp
	{
		static_cast<const ipport &>(remote)
	};

	if(!ipp && !remote.hostname)
	{
		const auto len{strlcpy(data(buf), "0.0.0.0", size(buf))};
		return { data(buf), size_t(len) };
	}
	else if(!ipp)
	{
		const auto len{strlcpy(data(buf), remote.hostname, size(buf))};
		return { data(buf), size_t(len) };
	}
	else return string(ipp, buf);
}

//
// remote
//

ircd::net::remote::remote(hostport hp)
:remote{std::move(hp.first), hp.second}
{
}

ircd::net::remote::remote(const string_view &host)
:remote
{
	std::string(host), "8448"s
}
{
}

ircd::net::remote::remote(const string_view &host,
                          const uint16_t &port)
:remote
{
	std::string(host), std::string(lex_cast(port))
}
{
}

ircd::net::remote::remote(const string_view &host,
                          const string_view &port)
:remote
{
	std::string(host), std::string(port)
}
{
}

ircd::net::remote::remote(std::string host)
:remote
{
	std::move(host), "8448"s
}
{
}

ircd::net::remote::remote(std::string host,
                          const uint16_t &port)
:ipport{host, port}
,hostname{std::move(host)}
{
}

ircd::net::remote::remote(std::string host,
                          const std::string &port)
:ipport{host, port}
,hostname{std::move(host)}
{
}

ircd::net::remote::remote(const ipport &ipp)
:ipport{ipp}
{
}

//
// ipport
//

ircd::net::ipport::ipport(const hostport &hp)
:ipport
{
	hp.first, std::string(lex_cast(port(hp)))
}
{
}

ircd::net::ipport::ipport(const string_view &host,
                          const uint16_t &port)
:ipport
{
	std::string(host), std::string(lex_cast(port))
}
{
}

ircd::net::ipport::ipport(const string_view &host,
                          const string_view &port)
:ipport
{
	std::string(host), std::string(port)
}
{
}

ircd::net::ipport::ipport(const std::string &host,
                          const uint16_t &port)
:ipport
{
	host, std::string{lex_cast(port)}
}
{
}

ircd::net::ipport::ipport(const std::string &host,
                          const std::string &port)
:ipport
{
	uint32_t{0},
	lex_cast<uint16_t>(port)
}
{
	assert(resolver);
	const ip::tcp::resolver::query query
	{
		host, port
	};

	auto epit
	{
		resolver->async_resolve(query, yield_context{to_asio{}})
	};

	static const ip::tcp::resolver::iterator end;
	if(epit == end)
		throw nxdomain("host '%s' not found", host);

	const ip::tcp::endpoint &ep
	{
		*epit
	};

	const asio::ip::address &address
	{
		ep.address()
	};

	std::get<TYPE>(*this) = address.is_v6();

	if(is_v6(*this))
	{
		std::get<IP>(*this) = address.to_v6().to_bytes();
		std::reverse(std::get<IP>(*this).begin(), std::get<IP>(*this).end());
	}
	else host4(*this) = address.to_v4().to_ulong();

	log.debug("resolved remote %s:%u => %s %s",
	          host,
	          net::port(*this),
	          is_v6(*this)? "IP6" : "IP4",
	          string(*this));
}

//
// hostport
//

const ircd::net::hostport
ircd::net::hostport::null
{
	"0.0.0.0"s, 0
};

ircd::net::hostport::hostport(std::string s,
                              const uint16_t &port)
try
:std::pair<std::string, uint16_t>
{
	std::move(s), port
}
{
	if(port != 8448)
		return;

	//TODO: ipv6
	const auto port_suffix
	{
		rsplit(first, ':').second
	};

	if(!port_suffix.empty() && port_suffix != "8448")
		second = lex_cast<uint16_t>(port_suffix);
}
catch(const std::exception &e)
{
	throw net::invalid_argument
	{
		"Supplied host name and/or port number: ", e.what()
	};
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
