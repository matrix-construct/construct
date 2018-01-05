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

/// Internal pimpl wrapping an instance of boost's resolver service. This
/// class is a singleton with the instance as a static member of
/// ircd::net::resolve. This service requires a valid ircd::ios which is not
/// available during static initialization; instead it is tied to net::init.
struct ircd::net::resolver
:std::unique_ptr<ip::tcp::resolver>
{
	resolver() = default;
	~resolver() noexcept = default;
};

///////////////////////////////////////////////////////////////////////////////
//
// net/net.h
//

/// Network subsystem log facility with dedicated SNOMASK.
struct ircd::log::log
ircd::net::log
{
	"net", 'N'
};

/// Network subsystem initialization
ircd::net::init::init()
{
	assert(ircd::ios);
	resolve::resolver.reset(new ip::tcp::resolver{*ircd::ios});
}

/// Network subsystem shutdown
ircd::net::init::~init()
{
	resolve::resolver.reset(nullptr);
}

//
// socket (public)
//

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

ircd::ctx::future<std::shared_ptr<ircd::net::socket>>
ircd::net::open(const hostport &hostport,
                const milliseconds &timeout)
{
	ctx::promise<std::shared_ptr<ircd::net::socket>> p;
	ctx::future<std::shared_ptr<ircd::net::socket>> f(p);
	resolve(hostport, [p(std::move(p)), timeout]
	(auto eptr, const ipport &ipport)
	mutable
	{
		if(eptr)
			return p.set_exception(std::move(eptr));

		const auto ep{make_endpoint(ipport)};
		const auto s(std::make_shared<socket>());
		s->open(ep, timeout, [p(std::move(p)), s]
		(const error_code &ec)
		mutable
		{
			if(ec)
			{
				disconnect(*s, dc::RST);
				p.set_exception(make_eptr(ec));
			}
			else p.set_value(s);
		});
	});

	return f;
}

ircd::ctx::future<std::shared_ptr<ircd::net::socket>>
ircd::net::open(const ipport &ipport,
                const milliseconds &timeout)
{
	ctx::promise<std::shared_ptr<ircd::net::socket>> p;
	ctx::future<std::shared_ptr<ircd::net::socket>> f(p);
	const auto ep{make_endpoint(ipport)};
	const auto s(std::make_shared<socket>());
	s->open(ep, timeout, [p(std::move(p)), s]
	(const error_code &ec)
	mutable
	{
		if(ec)
		{
			disconnect(*s, dc::RST);
			p.set_exception(make_eptr(ec));
		}
		else p.set_value(s);
	});

	return f;
}

std::shared_ptr<ircd::net::socket>
ircd::net::connect(const net::remote &remote,
                   const milliseconds &timeout)
{
	const auto ep{make_endpoint(remote)};
	return connect(ep, timeout);
}

std::shared_ptr<ircd::net::socket>
ircd::net::connect(const ip::tcp::endpoint &remote,
                   const milliseconds &timeout)
{
	const auto ret(std::make_shared<socket>());
	ret->open(remote, timeout);
	return ret;
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
                 const ilist<const_buffer> &bufs)
{
	const size_t wrote(socket.write(bufs));
	assert(wrote == size(bufs));
	return wrote;
}

ircd::const_raw_buffer
ircd::net::peer_cert_der(const mutable_raw_buffer &buf,
                         const socket &socket)
{
	const SSL &ssl(socket);
	const X509 &cert(openssl::get_peer_cert(ssl));
	return openssl::i2d(buf, cert);
}

ircd::net::ipport
ircd::net::local_ipport(const socket &socket)
noexcept try
{
    const auto &ep(socket.local());
	return make_ipport(ep);
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
	return make_ipport(ep);
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
	using boost::system::system_category;

	if(unlikely(interrupting))
		throw ctx::interrupted();

	if(likely(ec == success))
		return false;

	if(ec.category() == system_category()) switch(ec.value())
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
	using boost::system::system_category;
	using namespace boost::system::errc;

	if(unlikely(interrupting))
		throw ctx::interrupted();

	if(likely(ec == success))
		return false;

	if(ec.category() == system_category()) switch(ec.value())
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
/*
	ssl.set_options
	(
		//ssl.default_workarounds
		//| ssl.no_tlsv1
		//| ssl.no_tlsv1_1
		//| ssl.no_tlsv1_2
		//| ssl.no_sslv2
		//| ssl.no_sslv3
		//| ssl.single_dh_use
	);
*/
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
// socket::xfer
//

ircd::net::socket::xfer::xfer(const error_code &error_code,
                              const size_t &bytes)
:eptr{make_eptr(error_code)}
,bytes{bytes}
{
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

void
ircd::net::socket::open(const ip::tcp::endpoint &ep,
                        const milliseconds &timeout)
try
{
	const life_guard<socket> lg{*this};
	const scope_timeout ts{*this, timeout};
	log.debug("socket(%p) attempting connect to remote: %s for the next %ld$ms",
	          this,
	          string(ep),
	          timeout.count());

	connect(ep);
	log.debug("socket(%p) connected to remote: %s from local: %s; performing handshake...",
	          this,
	          string(ep),
	          string(local()));

	handshake(handshake_type::client);
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

/// Attempt to connect and ssl handshake; asynchronous, callback when done.
///
void
ircd::net::socket::open(const ip::tcp::endpoint &ep,
                        const milliseconds &timeout,
                        handler callback)
{
	log.debug("socket(%p) attempting connect to remote: %s for the next %ld$ms",
	          this,
	          string(ep),
	          timeout.count());

	auto handler
	{
		std::bind(&socket::handle_connect, this, weak_from(*this), std::move(callback), ph::_1)
	};

	this->connect(ep, std::move(handler));
	set_timeout(timeout);
}

void
ircd::net::socket::connect(const ip::tcp::endpoint &ep)
{
	sd.async_connect(ep, yield_context{to_asio{}});
}

void
ircd::net::socket::connect(const ip::tcp::endpoint &ep,
                           handler callback)
{
	sd.async_connect(ep, std::move(callback));
}

void
ircd::net::socket::handshake(const handshake_type &type)
{
	ssl.async_handshake(type, yield_context{to_asio{}});
}

void
ircd::net::socket::handshake(const handshake_type &type,
                             handler callback)
{
	ssl.async_handshake(type, std::move(callback));
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
		          ircd::string(remote_ipport(*this)),
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
			set_timeout(8s);
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
	static const auto good{[](const auto &ec)
	{
		return ec == boost::system::errc::success;
	}};

	boost::system::error_code ec[2];
	sd.cancel(ec[0]);
	timer.cancel(ec[1]);
	return std::all_of(begin(ec), end(ec), good);
}

/// Asynchronous callback when the socket is ready
///
/// Overload for operator() without a timeout. see: operator()
///
void
ircd::net::socket::operator()(const wait_type &type,
                              handler h)
{
	operator()(type, milliseconds(-1), std::move(h));
}

/// Asynchronous callback when the socket is ready
///
/// This function calls back the handler when the socket is ready
/// for the operation of the specified type.
///
void
ircd::net::socket::operator()(const wait_type &type,
                              const milliseconds &timeout,
                              handler callback)
{
	auto handle
	{
		std::bind(&socket::handle, this, weak_from(*this), std::move(callback), ph::_1)
	};

	assert(connected());
	switch(type)
	{
		case wait_type::wait_error:
		case wait_type::wait_write:
		{
			sd.async_wait(type, std::move(handle));
			break;
		}

		// There might be a bug in boost on linux which is only reproducible
		// when serving a large number of assets: a ready status for the socket
		// is not indicated when it ought to be, at random. This is fixed below
		// by doing it the old way (pre boost-1.66 sd.async_wait()) with the
		// proper peek.
		case wait_type::wait_read:
		{
			static const auto flags{ip::tcp::socket::message_peek};
			sd.async_receive(buffer::null_buffers, flags, std::move(handle));
			break;
		}
	}

	// Commit to timeout here in case exception was thrown earlier.
	set_timeout(timeout);
}

void
ircd::net::socket::handle(const std::weak_ptr<socket> wp,
                          const handler callback,
                          const error_code &ec)
noexcept try
{
	using namespace boost::system::errc;
	using boost::system::system_category;

	// After life_guard is constructed it is safe to use *this in this frame.
	const life_guard<socket> s{wp};
	log.debug("socket(%p): handle: (%s)",
	          this,
	          string(ec));

	if(!timedout)
		cancel_timeout();

	if(ec.category() == system_category()) switch(ec.value())
	{
		case operation_canceled:
			if(timedout)
				break;

			return;

		// This is a condition which we hide from the user.
		case bad_file_descriptor:
			return;

		// Everything else is passed up to the user.
		default:
			break;
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
catch(const std::exception &e)
{
	log.critical("socket(%p): handle: %s",
	             this,
	             e.what());
	assert(0);
}

void
ircd::net::socket::handle_connect(std::weak_ptr<socket> wp,
                                  handler callback,
                                  const error_code &ec)
noexcept try
{
	const life_guard<socket> s{wp};
	assert(!timedout || ec == boost::system::errc::operation_canceled);
	log.debug("socket(%p) connect from local: %s to remote: %s: %s",
	          this,
	          string(local_ipport(*this)),
	          string(remote_ipport(*this)),
	          string(ec));

	// A connect error
	if(ec)
	{
		cancel_timeout();
		call_user(callback, ec);
		return;
	}

	auto handler
	{
		std::bind(&socket::handle_handshake, this, wp, std::move(callback), ph::_1)
	};

	handshake(handshake_type::client, std::move(handler));
}
catch(const std::bad_weak_ptr &e)
{
	log.warning("socket(%p): belated callback to handle_connect... (%s)",
	            this,
	            e.what());
	assert(0);
}
catch(const std::exception &e)
{
	log.critical("socket(%p): handle_connect: %s",
	             this,
	             e.what());
	assert(0);
}

void
ircd::net::socket::handle_handshake(std::weak_ptr<socket> wp,
                                    handler callback,
                                    const error_code &ec)
noexcept try
{
	const life_guard<socket> s{wp};
	assert(!timedout || ec == boost::system::errc::operation_canceled);
	log.debug("socket(%p) handshake from local: %s to remote: %s: %s",
	          this,
	          string(local_ipport(*this)),
	          string(remote_ipport(*this)),
	          string(ec));

	if(!timedout)
		cancel_timeout();

	call_user(callback, ec);
}
catch(const std::bad_weak_ptr &e)
{
	log.warning("socket(%p): belated callback to handle_handshake... (%s)",
	            this,
	            e.what());
	assert(0);
}
catch(const std::exception &e)
{
	log.critical("socket(%p): handle_handshake: %s",
	             this,
	             e.what());
	assert(0);
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
			assert(timedout == false);
			timedout = true;
			sd.cancel();
			break;
		}

		// A cancelation means there was no timeout.
		case operation_canceled:
		{
			assert(ec.category() == boost::system::system_category());
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
	log.critical("socket(%p): handle_timeout: unexpected: %s\n",
	             (const void *)this,
	              e.what());
	assert(0);
}
catch(const std::exception &e)
{
	log.error("socket(%p): handle timeout: %s",
	          (const void *)this,
	          e.what());
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

void
ircd::net::socket::flush()
{
	if(nodelay())
		return;

	nodelay(true);
	nodelay(false);
}

void
ircd::net::socket::nodelay(const bool &b)
{
	ip::tcp::no_delay option{b};
	sd.set_option(option);
}

void
ircd::net::socket::blocking(const bool &b)
{
	sd.non_blocking(b);
}

void
ircd::net::socket::wlowat(const size_t &bytes)
{
	assert(bytes <= std::numeric_limits<int>::max());
	ip::tcp::socket::send_low_watermark option
	{
		int(bytes)
	};

	sd.set_option(option);
}

void
ircd::net::socket::rlowat(const size_t &bytes)
{
	assert(bytes <= std::numeric_limits<int>::max());
	ip::tcp::socket::receive_low_watermark option
	{
		int(bytes)
	};

	sd.set_option(option);
}

void
ircd::net::socket::wbufsz(const size_t &bytes)
{
	assert(bytes <= std::numeric_limits<int>::max());
	ip::tcp::socket::send_buffer_size option
	{
		int(bytes)
	};

	sd.set_option(option);
}

void
ircd::net::socket::rbufsz(const size_t &bytes)
{
	assert(bytes <= std::numeric_limits<int>::max());
	ip::tcp::socket::receive_buffer_size option
	{
		int(bytes)
	};

	sd.set_option(option);
}

bool
ircd::net::socket::nodelay()
const
{
	ip::tcp::no_delay option;
	sd.get_option(option);
	return option.value();
}

bool
ircd::net::socket::blocking()
const
{
	return !sd.non_blocking();
}

size_t
ircd::net::socket::wlowat()
const
{
	ip::tcp::socket::send_low_watermark option{};
	sd.get_option(option);
	return option.value();
}

size_t
ircd::net::socket::rlowat()
const
{
	ip::tcp::socket::receive_low_watermark option{};
	sd.get_option(option);
	return option.value();
}

size_t
ircd::net::socket::wbufsz()
const
{
	ip::tcp::socket::send_buffer_size option{};
	sd.get_option(option);
	return option.value();
}

size_t
ircd::net::socket::rbufsz()
const
{
	ip::tcp::socket::receive_buffer_size option{};
	sd.get_option(option);
	return option.value();
}

size_t
ircd::net::socket::readable()
const
{
	ip::tcp::socket::bytes_readable command{true};
	const_cast<ip::tcp::socket &>(sd).io_control(command);
	return command.get();
}

size_t
ircd::net::socket::available()
const
{
	return sd.available();
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

ircd::milliseconds
ircd::net::socket::cancel_timeout()
noexcept
{
	const auto ret
	{
		timer.expires_from_now()
	};

	boost::system::error_code ec;
	timer.cancel(ec);
	assert(!ec);

	return duration_cast<milliseconds>(ret);
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
// net/resolve.h
//

namespace ircd::net
{
	// Internal resolve base (requires boost syms)
	using resolve_callback = std::function<void (std::exception_ptr, ip::tcp::resolver::results_type)>;
	void _resolve(const hostport &, ip::tcp::resolver::flags, resolve_callback);
	void _resolve(const ipport &, resolve_callback);
}

/// Singleton instance of the public interface ircd::net::resolve
decltype(ircd::net::resolve)
ircd::net::resolve
{
};

/// Singleton instance of the internal boost resolver wrapper.
decltype(ircd::net::resolve::resolver)
ircd::net::resolve::resolver
{
};

/// Resolve a numerical address to a hostname string. This is a PTR record
/// query or 'reverse DNS' lookup.
ircd::ctx::future<std::string>
ircd::net::resolve::operator()(const ipport &ipport)
{
	ctx::promise<std::string> p;
	ctx::future<std::string> ret{p};
	operator()(ipport, [p(std::move(p))]
	(std::exception_ptr eptr, std::string ptr)
	mutable
	{
		if(eptr)
			p.set_exception(std::move(eptr));
		else
			p.set_value(ptr);
	});

	return ret;
}

/// Resolve a hostname (with service name/portnum) to a numerical address. This
/// is an A or AAAA query (with automatic SRV query) returning a single result.
ircd::ctx::future<ircd::net::ipport>
ircd::net::resolve::operator()(const hostport &hostport)
{
	ctx::promise<ipport> p;
	ctx::future<ipport> ret{p};
	operator()(hostport, [p(std::move(p))]
	(std::exception_ptr eptr, const ipport &ip)
	mutable
	{
		if(eptr)
			p.set_exception(std::move(eptr));
		else
			p.set_value(ip);
	});

	return ret;
}

/// Lower-level PTR query (i.e "reverse DNS") with asynchronous callback
/// interface.
void
ircd::net::resolve::operator()(const ipport &ipport,
                               callback_reverse callback)
{
	_resolve(ipport, [callback(std::move(callback))]
	(std::exception_ptr eptr, ip::tcp::resolver::results_type results)
	{
		if(eptr)
			return callback(std::move(eptr), {});

		if(results.empty())
			return callback({}, {});

		assert(results.size() <= 1);
		const auto &result(*begin(results));
		callback({}, result.host_name());
	});
}

/// Lower-level A or AAAA query (with automatic SRV query) with asynchronous
/// callback interface. This returns only one result.
void
ircd::net::resolve::operator()(const hostport &hostport,
                               callback_one callback)
{
	static const ip::tcp::resolver::flags flags{};
	_resolve(hostport, flags, [callback(std::move(callback))]
	(std::exception_ptr eptr, ip::tcp::resolver::results_type results)
	{
		if(eptr)
			return callback(std::move(eptr), {});

		if(results.empty())
			return callback(std::make_exception_ptr(nxdomain{}), {});

		const auto &result(*begin(results));
		callback(std::move(eptr), make_ipport(result));
	});
}

/// Lower-level A+AAAA query (with automatic SRV query). This returns a vector
/// of all results in the callback.
void
ircd::net::resolve::operator()(const hostport &hostport,
                               callback_many callback)
{
	static const ip::tcp::resolver::flags flags{};
	_resolve(hostport, flags, [callback(std::move(callback))]
	(std::exception_ptr eptr, ip::tcp::resolver::results_type results)
	{
		if(eptr)
			return callback(std::move(eptr), {});

		std::vector<ipport> vector(results.size());
		std::transform(begin(results), end(results), begin(vector), []
		(const auto &entry)
		{
			return make_ipport(entry.endpoint());
		});

		callback(std::move(eptr), std::move(vector));
	});
}

/// Internal A/AAAA record resolver function
void
ircd::net::_resolve(const hostport &hostport,
                    ip::tcp::resolver::flags flags,
                    resolve_callback callback)
{
	// Trivial host string
	const string_view &host
	{
		hostport.host
	};

	// Determine if the port is a string or requires a lex_cast to one.
	char portbuf[8];
	const string_view &port
	{
		hostport.portnum? lex_cast(hostport.portnum, portbuf) : hostport.port
	};

	// Determine if the port is numeric and hint to avoid name lookup if so.
	if(hostport.portnum || ctype<std::isdigit>(hostport.port) == -1)
		flags |= ip::tcp::resolver::numeric_service;

	// This base handler will provide exception guarantees for the entire stack.
	// It may invoke callback twice in the case when callback throws unhandled,
	// but the latter invocation will always have an the eptr set.
	assert(bool(ircd::net::resolve::resolver));
	resolve::resolver->async_resolve(host, port, flags, [callback(std::move(callback))]
	(const error_code &ec, ip::tcp::resolver::results_type results)
	noexcept
	{
		if(ec)
		{
			callback(std::make_exception_ptr(boost::system::system_error{ec}), std::move(results));
		}
		else try
		{
			callback({}, std::move(results));
		}
		catch(...)
		{
			callback(std::make_exception_ptr(std::current_exception()), {});
		}
	});
}

/// Internal PTR record resolver function
void
ircd::net::_resolve(const ipport &ipport,
                    resolve_callback callback)
{
	assert(bool(ircd::net::resolve::resolver));
	resolve::resolver->async_resolve(make_endpoint(ipport), [callback(std::move(callback))]
	(const error_code &ec, ip::tcp::resolver::results_type results)
	noexcept
	{
		if(ec)
		{
			callback(std::make_exception_ptr(boost::system::system_error{ec}), std::move(results));
		}
		else try
		{
			callback({}, std::move(results));
		}
		catch(...)
		{
			callback(std::make_exception_ptr(std::current_exception()), {});
		}
	});
}

///////////////////////////////////////////////////////////////////////////////
//
// net/remote.h
//

//
// host / port utils
//

ircd::string_view
ircd::net::string(const mutable_buffer &buf,
                  const uint32_t &ip)
{
	const auto len
	{
		ip::address_v4{ip}.to_string().copy(data(buf), size(buf))
	};

	return { data(buf), size_t(len) };
}

ircd::string_view
ircd::net::string(const mutable_buffer &buf,
                  const uint128_t &ip)
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
		ip::address_v6{punpun}.to_string().copy(data(buf), size(buf))
	};

	return { data(buf), size_t(len) };
}

ircd::string_view
ircd::net::string(const mutable_buffer &buf,
                  const hostport &hp)
{
	const auto len
	{
		fmt::sprintf
		{
			buf, "%s:%s",
			hp.host,
			hp.portnum? lex_cast(hp.portnum) : hp.port
		}
	};

	return { data(buf), size_t(len) };
}

ircd::string_view
ircd::net::string(const mutable_buffer &buf,
                  const ipport &ipp)
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
ircd::net::string(const mutable_buffer &buf,
                  const remote &remote)
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
	else return string(buf, ipp);
}

//
// remote
//

ircd::net::remote::remote(const hostport &hostport)
:ipport
{
	resolve(hostport)
}
,hostname
{
	hostport.host
}
{
}

std::ostream &
ircd::net::operator<<(std::ostream &s, const remote &t)
{
	char buf[256];
	s << string(buf, t);
	return s;
}

//
// ipport
//

std::ostream &
ircd::net::operator<<(std::ostream &s, const ipport &t)
{
	char buf[256];
	s << string(buf, t);
	return s;
}

ircd::net::ipport
ircd::net::make_ipport(const boost::asio::ip::tcp::endpoint &ep)
{
	return ipport
	{
		ep.address(), ep.port()
	};
}

boost::asio::ip::tcp::endpoint
ircd::net::make_endpoint(const ipport &ipport)
{
	return
	{
		is_v6(ipport)? ip::tcp::endpoint
		{
			asio::ip::address_v6 { std::get<ipport.IP>(ipport) }, port(ipport)
		}
		: ip::tcp::endpoint
		{
			asio::ip::address_v4 { host4(ipport) }, port(ipport)
		},
	};
}

ircd::net::ipport::ipport(const boost::asio::ip::address &address,
                          const uint16_t &port)
{
	std::get<TYPE>(*this) = address.is_v6();

	if(is_v6(*this))
	{
		std::get<IP>(*this) = address.to_v6().to_bytes();
		std::reverse(std::get<IP>(*this).begin(), std::get<IP>(*this).end());
	}
	else host4(*this) = address.to_v4().to_ulong();

	net::port(*this) = port;
}

//
// hostport
//

std::ostream &
ircd::net::operator<<(std::ostream &s, const hostport &t)
{
	char buf[256];
	s << string(buf, t);
	return s;
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

///////////////////////////////////////////////////////////////////////////////
//
// asio.h
//

std::exception_ptr
ircd::make_eptr(const boost::system::error_code &ec)
{
	return bool(ec)? std::make_exception_ptr(boost::system::system_error(ec)):
	                 std::exception_ptr{};
}

std::string
ircd::string(const boost::system::system_error &e)
{
	return string(e.code());
}

std::string
ircd::string(const boost::system::error_code &ec)
{
	std::string ret(128, char{});
	ret.resize(string(mutable_buffer{ret}, ec).size());
	return ret;
}

ircd::string_view
ircd::string(const mutable_buffer &buf,
             const boost::system::system_error &e)
{
	return string(buf, e.code());
}

ircd::string_view
ircd::string(const mutable_buffer &buf,
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
