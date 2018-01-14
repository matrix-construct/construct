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
	sslv23_client.set_verify_mode(asio::ssl::verify_peer);
	sslv23_client.set_default_verify_paths();
}

/// Network subsystem shutdown
ircd::net::init::~init()
{
	resolve::resolver.reset(nullptr);
}

ircd::const_raw_buffer
ircd::net::peer_cert_der(const mutable_raw_buffer &buf,
                         const socket &socket)
{
	const SSL &ssl(socket);
	const X509 &cert{openssl::peer_cert(ssl)};
	return openssl::i2d(buf, cert);
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

size_t
ircd::net::available(const socket &socket)
noexcept
{
	const ip::tcp::socket &sd(socket);
	boost::system::error_code ec;
	return sd.available(ec);
}

size_t
ircd::net::readable(const socket &socket)
{
	ip::tcp::socket &sd(const_cast<net::socket &>(socket));
	ip::tcp::socket::bytes_readable command{true};
	sd.io_control(command);
	return command.get();
}

bool
ircd::net::connected(const socket &socket)
noexcept try
{
	const ip::tcp::socket &sd(socket);
	return sd.is_open();
}
catch(...)
{
	return false;
}

///////////////////////////////////////////////////////////////////////////////
//
// net/write.h
//

void
ircd::net::flush(socket &socket)
{
	if(nodelay(socket))
		return;

	nodelay(socket, true);
	nodelay(socket, false);
}

/// Yields ircd::ctx until all buffers are sent.
///
/// This is blocking behavior; use this if the following are true:
///
/// * You put a timer on the socket so if the remote slows us down the data
/// will not occupy the daemon's memory for a long time. Remember, *all* of
/// the data will be sitting in memory even after some of it was ack'ed by
/// the remote.
///
/// * You are willing to dedicate the ircd::ctx to sending all the data to
/// the remote. The ircd::ctx will be yielding until everything is sent.
///
size_t
ircd::net::write_all(socket &socket,
                     const vector_view<const const_buffer> &buffers)
{
	return socket.write_all(buffers);
}

/// Writes as much as possible until one of the following is true:
///
/// * The kernel buffer for the socket is full.
/// * The user buffer is exhausted.
///
/// This is non-blocking behavior. No yielding will take place; no timer is
/// needed. Multiple syscalls will be composed to fulfill the above points.
///
size_t
ircd::net::write_any(socket &socket,
                     const vector_view<const const_buffer> &buffers)
{
	return socket.write_any(buffers);
}

/// Writes one "unit" of data or less; never more. The size of that unit
/// is determined by the system. Less may be written if one of the following
/// is true:
///
/// * The kernel buffer for the socket is full.
/// * The user buffer is exhausted.
///
/// If neither are true, more can be written using additional calls;
/// alternatively, use other variants of write_ for that.
///
/// This is non-blocking behavior. No yielding will take place; no timer is
/// needed. Only one syscall will occur.
///
size_t
ircd::net::write_one(socket &socket,
                     const vector_view<const const_buffer> &buffers)
{
	return socket.write_one(buffers);
}

///////////////////////////////////////////////////////////////////////////////
//
// net/read.h
//

/// Yields ircd::ctx until len bytes have been received and discarded from the
/// socket.
///
size_t
ircd::net::discard_all(socket &socket,
                       const size_t &len)
{
	static char buffer[512] alignas(16);

	size_t remain{len}; while(remain)
	{
		const mutable_buffer mb
		{
			buffer, std::min(remain, sizeof(buffer))
		};

		__builtin_prefetch(data(mb), 1, 0);    // 1 = write, 0 = no cache
		remain -= read_all(socket, mb);
	}

	return len;
}

/// Yields ircd::ctx until buffers are full.
///
/// Use this only if the following are true:
///
/// * You know the remote has made a guarantee to send you a specific amount
/// of data.
///
/// * You put a timer on the socket so that if the remote runs short this
/// call doesn't hang the ircd::ctx forever, otherwise it will until cancel.
///
/// * You are willing to dedicate the ircd::ctx to just this operation for
/// that amount of time.
///
size_t
ircd::net::read_all(socket &socket,
                    const vector_view<const mutable_buffer> &buffers)
{
	return socket.read_all(buffers);
}

/// Yields ircd::ctx until remote has sent at least one frame. The buffers may
/// be filled with any amount of data depending on what has accumulated.
///
/// Use this if the following are true:
///
/// * You know there is data to be read; you can do this asynchronously with
/// other features of the socket. Otherwise this will hang the ircd::ctx.
///
/// * You are willing to dedicate the ircd::ctx to just this operation,
/// which is non-blocking if data is known to be available, but may be
/// blocking if this call is made in the blind.
///
size_t
ircd::net::read_any(socket &socket,
                    const vector_view<const mutable_buffer> &buffers)
{
	return socket.read_any(buffers);
}

/// Reads one message or less in a single syscall. Non-blocking behavior.
///
/// This is intended for lowest-level/custom control and not preferred by
/// default.
///
size_t
ircd::net::read_one(socket &socket,
                    const vector_view<const mutable_buffer> &buffers)
{
	return socket.read_one(buffers);
}

///////////////////////////////////////////////////////////////////////////////
//
// net/wait.h
//

ircd::net::wait_opts
const ircd::net::wait_opts_default
{
};

/// Wait for socket to become "ready" using a ctx::future.
ircd::ctx::future<void>
ircd::net::wait(use_future_t,
                socket &socket,
                const wait_opts &wait_opts)
{
	ctx::promise<void> p;
	ctx::future<void> f{p};
	wait(socket, wait_opts, [p(std::move(p))]
	(std::exception_ptr eptr)
	mutable
	{
		if(eptr)
			p.set_exception(std::move(eptr));
		else
			p.set_value();
	});

	return f;
}

/// Wait for socket to become "ready"; yields ircd::ctx returning code.
ircd::error_code
ircd::net::wait(nothrow_t,
                socket &socket,
                const wait_opts &wait_opts)
try
{
	wait(socket, wait_opts);
	return {};
}
catch(const boost::system::system_error &e)
{
	return e.code();
}

/// Wait for socket to become "ready"; yields ircd::ctx; throws errors.
void
ircd::net::wait(socket &socket,
                const wait_opts &wait_opts)
{
	socket.wait(wait_opts);
}

/// Wait for socket to become "ready"; callback with exception_ptr
void
ircd::net::wait(socket &socket,
                const wait_opts &wait_opts,
                wait_callback_eptr callback)
{
	socket.wait(wait_opts, std::move(callback));
}

void
ircd::net::wait(socket &socket,
                const wait_opts &wait_opts,
                wait_callback_ec callback)
{
	socket.wait(wait_opts, std::move(callback));
}

ircd::string_view
ircd::net::reflect(const ready &type)
{
	switch(type)
	{
		case ready::ANY:     return "ANY"_sv;
		case ready::READ:    return "READ"_sv;
		case ready::WRITE:   return "WRITE"_sv;
		case ready::ERROR:   return "ERROR"_sv;
	}

	return "????"_sv;
}

///////////////////////////////////////////////////////////////////////////////
//
// net/close.h
//

/// Static instance of default close options.
ircd::net::close_opts
const ircd::net::close_opts_default
{
};

/// Static helper callback which may be passed to the callback-based overload
/// of close(). This callback does nothing.
ircd::net::close_callback
const ircd::net::close_ignore{[]
(std::exception_ptr eptr)
{
	return;
}};

ircd::ctx::future<void>
ircd::net::close(socket &socket,
                 const close_opts &opts)
{
	ctx::promise<void> p;
	ctx::future<void> f(p);
	close(socket, opts, [p(std::move(p))]
	(std::exception_ptr eptr)
	mutable
	{
		if(eptr)
			p.set_exception(std::move(eptr));
		else
			p.set_value();
	});

	return f;
}

void
ircd::net::close(socket &socket,
                 const close_opts &opts,
                 close_callback callback)
{
	socket.disconnect(opts, std::move(callback));
}

///////////////////////////////////////////////////////////////////////////////
//
// net/open.h
//

/// Open new socket with future-based report.
///
ircd::ctx::future<std::shared_ptr<ircd::net::socket>>
ircd::net::open(const open_opts &opts)
{
	ctx::promise<std::shared_ptr<socket>> p;
	ctx::future<std::shared_ptr<socket>> f(p);
	auto s{std::make_shared<socket>()};
	open(*s, opts, [s, p(std::move(p))]
	(std::exception_ptr eptr)
	mutable
	{
		if(eptr)
			p.set_exception(std::move(eptr));
		else
			p.set_value(s);
	});

	return f;
}

/// Open existing socket with callback-based report.
///
std::shared_ptr<ircd::net::socket>
ircd::net::open(const open_opts &opts,
                open_callback handler)
{
	auto s{std::make_shared<socket>()};
	open(*s, opts, std::move(handler));
	return s;
}

/// Open existing socket with callback-based report.
///
void
ircd::net::open(socket &socket,
                const open_opts &opts,
                open_callback handler)
{
	auto complete{[s(shared_from(socket)), handler(std::move(handler))]
	(std::exception_ptr eptr)
	{
		if(eptr)
			close(*s, dc::RST);

		handler(std::move(eptr));
	}};

	auto connector{[&socket, opts, complete(std::move(complete))]
	(std::exception_ptr eptr, const ipport &ipport)
	{
		if(eptr)
			return complete(std::move(eptr));

		const auto ep{make_endpoint(ipport)};
		socket.connect(ep, opts, std::move(complete));
	}};

	if(!opts.ipport)
		resolve(opts.hostport, std::move(connector));
	else
		connector({}, opts.ipport);
}

///////////////////////////////////////////////////////////////////////////////
//
// net/sopts.h
//

/// Construct sock_opts with the current options from socket argument
ircd::net::sock_opts::sock_opts(const socket &socket)
:blocking{net::blocking(socket)}
,nodelay{net::nodelay(socket)}
,keepalive{net::keepalive(socket)}
,linger{net::linger(socket)}
,read_bufsz{ssize_t(net::read_bufsz(socket))}
,write_bufsz{ssize_t(net::write_bufsz(socket))}
,read_lowat{ssize_t(net::read_lowat(socket))}
,write_lowat{ssize_t(net::write_lowat(socket))}
{
}

/// Updates the socket with provided options. Defaulted / -1'ed options are
/// ignored for updating.
void
ircd::net::set(socket &socket,
               const sock_opts &opts)
{
	if(opts.blocking != opts.IGN)
		net::blocking(socket, opts.blocking);

	if(opts.nodelay != opts.IGN)
		net::nodelay(socket, opts.nodelay);

	if(opts.keepalive != opts.IGN)
		net::keepalive(socket, opts.keepalive);

	if(opts.linger != opts.IGN)
		net::linger(socket, opts.linger);

	if(opts.read_bufsz != opts.IGN)
		net::read_bufsz(socket, opts.read_bufsz);

	if(opts.write_bufsz != opts.IGN)
		net::write_bufsz(socket, opts.write_bufsz);

	if(opts.read_lowat != opts.IGN)
		net::read_lowat(socket, opts.read_lowat);

	if(opts.write_lowat != opts.IGN)
		net::write_lowat(socket, opts.write_lowat);
}

void
ircd::net::write_lowat(socket &socket,
                       const size_t &bytes)
{
	assert(bytes <= std::numeric_limits<int>::max());
	ip::tcp::socket::send_low_watermark option
	{
		int(bytes)
	};

	ip::tcp::socket &sd(socket);
	sd.set_option(option);
}

void
ircd::net::read_lowat(socket &socket,
                      const size_t &bytes)
{
	assert(bytes <= std::numeric_limits<int>::max());
	ip::tcp::socket::receive_low_watermark option
	{
		int(bytes)
	};

	ip::tcp::socket &sd(socket);
	sd.set_option(option);
}

void
ircd::net::write_bufsz(socket &socket,
                       const size_t &bytes)
{
	assert(bytes <= std::numeric_limits<int>::max());
	ip::tcp::socket::send_buffer_size option
	{
		int(bytes)
	};

	ip::tcp::socket &sd(socket);
	sd.set_option(option);
}

void
ircd::net::read_bufsz(socket &socket,
                      const size_t &bytes)
{
	assert(bytes <= std::numeric_limits<int>::max());
	ip::tcp::socket::receive_buffer_size option
	{
		int(bytes)
	};

	ip::tcp::socket &sd(socket);
	sd.set_option(option);
}

void
ircd::net::linger(socket &socket,
                  const time_t &t)
{
	assert(t >= std::numeric_limits<int>::min());
	assert(t <= std::numeric_limits<int>::max());
	ip::tcp::socket::linger option
	{
		t >= 0,                // ON / OFF boolean
		t >= 0? int(t) : 0     // Uses 0 when OFF
	};

	ip::tcp::socket &sd(socket);
	sd.set_option(option);
}

void
ircd::net::keepalive(socket &socket,
                     const bool &b)
{
	ip::tcp::socket::keep_alive option{b};
	ip::tcp::socket &sd(socket);
	sd.set_option(option);
}

void
ircd::net::nodelay(socket &socket,
                   const bool &b)
{
	ip::tcp::no_delay option{b};
	ip::tcp::socket &sd(socket);
	sd.set_option(option);
}

/// Toggles the behavior of non-async asio calls.
///
/// This option affects very little in practice and only sets a flag in
/// userspace in asio, not an actual ioctl(). Specifically:
///
/// * All sockets are already set by asio to FIONBIO=1 no matter what, thus
/// nothing really blocks the event loop ever by default unless you try hard.
///
/// * All asio::async_ and sd.async_ and ssl.async_ calls will always do what
/// the synchronous/blocking alternative would have accomplished but using
/// the async methodology. i.e if a buffer is full you will always wait
/// asynchronously: async_write() will wait for everything, async_write_some()
/// will wait for something, etc -- but there will never be true non-blocking
/// _effective behavior_ from these calls.
///
/// * All asio non-async calls conduct blocking by (on linux) poll()'ing the
/// socket to get a real kernel-blocking operation out of it (this is the
/// try-hard part).
///
/// This flag only controls the behavior of the last bullet. In practice,
/// in this project there is never a reason to ever set this to true,
/// however, sockets do get constructed by asio in blocking mode by default
/// so we mostly use this function to set it to non-blocking.
///
void
ircd::net::blocking(socket &socket,
                    const bool &b)
{
	ip::tcp::socket &sd(socket);
	sd.non_blocking(!b);
}

size_t
ircd::net::write_lowat(const socket &socket)
{
	const ip::tcp::socket &sd(socket);
	ip::tcp::socket::send_low_watermark option{};
	sd.get_option(option);
	return option.value();
}

size_t
ircd::net::read_lowat(const socket &socket)
{
	const ip::tcp::socket &sd(socket);
	ip::tcp::socket::receive_low_watermark option{};
	sd.get_option(option);
	return option.value();
}

size_t
ircd::net::write_bufsz(const socket &socket)
{
	const ip::tcp::socket &sd(socket);
	ip::tcp::socket::send_buffer_size option{};
	sd.get_option(option);
	return option.value();
}

size_t
ircd::net::read_bufsz(const socket &socket)
{
	const ip::tcp::socket &sd(socket);
	ip::tcp::socket::receive_buffer_size option{};
	sd.get_option(option);
	return option.value();
}

time_t
ircd::net::linger(const socket &socket)
{
	const ip::tcp::socket &sd(socket);
	ip::tcp::socket::linger option;
	sd.get_option(option);
	return option.enabled()? option.timeout() : -1;
}

bool
ircd::net::keepalive(const socket &socket)
{
	const ip::tcp::socket &sd(socket);
	ip::tcp::socket::keep_alive option;
	sd.get_option(option);
	return option.value();
}

bool
ircd::net::nodelay(const socket &socket)
{
	const ip::tcp::socket &sd(socket);
	ip::tcp::no_delay option;
	sd.get_option(option);
	return option.value();
}

bool
ircd::net::blocking(const socket &socket)
{
	const ip::tcp::socket &sd(socket);
	return !sd.non_blocking();
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
	void check_handshake_error(const error_code &ec, socket &);
	void handshake(const error_code &ec, std::shared_ptr<socket>, std::weak_ptr<acceptor>) noexcept;

	// Acceptance stack
	bool check_accept_error(const error_code &ec, socket &);
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
	log.error("acceptor(%p) join: %s",
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
	log.error("acceptor(%p) interrupt: %s",
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
	++accepting;
	ip::tcp::socket &sd(*sock);
	a.async_accept(sd, std::bind(&acceptor::accept, this, ph::_1, sock, weak_from(*this)));
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
	assert(bool(sock));
	log.debug("%s: socket(%p) accepted(%zu) %s %s",
	          std::string(*this),
	          sock.get(),
	          accepting,
	          string(remote_ipport(*sock)),
	          string(ec));

	if(!check_accept_error(ec, *sock))
		return;

	// Toggles the behavior of non-async functions; see func comment
	blocking(*sock, false);

	static const socket::handshake_type handshake_type
	{
		socket::handshake_type::server
	};

	auto handshake
	{
		std::bind(&acceptor::handshake, this, ph::_1, sock, a)
	};

	++handshaking;
	sock->set_timeout(5000ms); //TODO: config
	sock->ssl.async_handshake(handshake_type, std::move(handshake));
}
catch(const ctx::interrupted &e)
{
	log.debug("%s: acceptor interrupted socket(%p) %s",
	          std::string(*this),
	          sock.get(),
	          string(ec));

	joining.notify_all();
}
catch(const std::exception &e)
{
	log.error("%s: socket(%p) in accept(): %s",
	          std::string(*this),
	          sock.get(),
	          e.what());
	throw;
}

/// Error handler for the accept socket callback. This handler determines
/// whether or not the handler should return or continue processing the
/// result.
///
bool
ircd::net::listener::acceptor::check_accept_error(const error_code &ec,
                                                  socket &sock)
{
	using namespace boost::system::errc;
	using boost::system::system_category;

	if(unlikely(interrupting))
		throw ctx::interrupted();

	if(likely(ec == success))
	{
		this->next();
		return true;
	}

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
	const unwind::exceptional drop{[&sock]
	{
		if(bool(sock))
			close(*sock, dc::RST, close_ignore);
	}};

	assert(bool(sock));
	log.debug("socket(%p) local[%s] remote[%s] handshook(%zu) %s",
	          sock.get(),
	          string(local_ipport(*sock)),
	          string(remote_ipport(*sock)),
	          handshaking,
	          string(ec));

	check_handshake_error(ec, *sock);
	sock->cancel_timeout();
	add_client(sock);
}
catch(const ctx::interrupted &e)
{
	log.debug("%s: SSL handshake interrupted socket(%p) %s",
	          std::string(*this),
	          sock.get(),
	          string(ec));

	joining.notify_all();
}
catch(const std::exception &e)
{
	log.error("%s: socket(%p) in handshake(): %s",
	          std::string(*this),
	          sock.get(),
	          e.what());
}

/// Error handler for the SSL handshake callback. This handler determines
/// whether or not the handler should return or continue processing the
/// result.
///
void
ircd::net::listener::acceptor::check_handshake_error(const error_code &ec,
                                                     socket &sock)
{
	using boost::system::system_category;
	using namespace boost::system::errc;

	if(unlikely(interrupting))
		throw ctx::interrupted();

	if(likely(ec == success))
		return;

	if(ec.category() == system_category()) switch(ec.value())
	{
		case operation_canceled:
			break;

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
		0
		//| ssl.default_workarounds
		//| ssl.no_tlsv1
		//| ssl.no_tlsv1_1
		//| ssl.no_tlsv1_2
		//| ssl.no_sslv2
		//| ssl.no_sslv3
		//| ssl.single_dh_use
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
	if(unlikely(RB_DEBUG_LEVEL && connected(*this)))
		log.critical("Failed to ensure socket(%p) is disconnected from %s before dtor.",
		             this,
		             string(remote()));

	assert(!connected(*this));
}
catch(const std::exception &e)
{
	log.critical("socket(%p) close: %s", this, e.what());
	return;
}

void
ircd::net::socket::connect(const endpoint &ep,
                           const open_opts &opts,
                           eptr_handler callback)
{
	log.debug("socket(%p) attempting connect remote[%s] to:%ld$ms",
	          this,
	          string(ep),
	          opts.connect_timeout.count());

	auto connect_handler
	{
		std::bind(&socket::handle_connect, this, weak_from(*this), opts, std::move(callback), ph::_1)
	};

	set_timeout(opts.connect_timeout);
	sd.async_connect(ep, std::move(connect_handler));
}

void
ircd::net::socket::handshake(const open_opts &opts,
                             eptr_handler callback)
{
	log.debug("socket(%p) local[%s] remote[%s] handshaking for '%s' to:%ld$ms",
	          this,
	          string(local_ipport(*this)),
	          string(remote_ipport(*this)),
	          common_name(opts),
	          opts.handshake_timeout.count());

	auto handshake_handler
	{
		std::bind(&socket::handle_handshake, this, weak_from(*this), std::move(callback), ph::_1)
	};

	auto verify_handler
	{
		std::bind(&socket::handle_verify, this, ph::_1, ph::_2, opts)
	};

	set_timeout(opts.handshake_timeout);
	ssl.set_verify_callback(std::move(verify_handler));
	ssl.async_handshake(handshake_type::client, std::move(handshake_handler));
}

void
ircd::net::socket::disconnect(const close_opts &opts,
                              eptr_handler callback)
try
{
	if(!sd.is_open())
	{
		call_user(callback, {});
		return;
	}

	log.debug("socket(%p) local[%s] remote[%s] disconnect type:%d user: in:%zu out:%zu",
	          (const void *)this,
	          string(local_ipport(*this)),
	          string(remote_ipport(*this)),
	          uint(opts.type),
	          in.bytes,
	          out.bytes);

	if(opts.sopts)
		set(*this, *opts.sopts);

	switch(opts.type)
	{
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
			auto disconnect_handler
			{
				std::bind(&socket::handle_disconnect, this, shared_from(*this), std::move(callback), ph::_1)
			};

			cancel();
			set_timeout(opts.timeout);
			ssl.async_shutdown(std::move(disconnect_handler));
			return;
		}
	}

	call_user(callback, {});
}
catch(const boost::system::system_error &e)
{
	call_user(callback, e.code());
}
catch(const std::exception &e)
{
	log.critical("socket(%p) disconnect: type: %d: %s",
	             (const void *)this,
	             uint(opts.type),
	             e.what());
	throw;
}

void
ircd::net::socket::cancel()
noexcept
{
	boost::system::error_code ec;

	sd.cancel(ec);
	assert(!ec);

	timer.cancel(ec);
	assert(!ec);
}

void
ircd::net::socket::wait(const wait_opts &opts,
                        wait_callback_eptr callback)
{
	wait(opts, [callback(std::move(callback))]
	(const error_code &ec)
	{
		callback(make_eptr(ec));
	});
}

/// Asynchronous callback when the socket is ready
///
/// Overload for operator() without a timeout. see: operator()
///
void
ircd::net::socket::wait(const wait_opts &opts)
{
	const scope_timeout timeout
	{
		*this, opts.timeout
	};

	switch(opts.type)
	{
		case ready::ERROR:
			sd.async_wait(wait_type::wait_error, yield_context{to_asio{}});
			break;

		case ready::WRITE:
			sd.async_wait(wait_type::wait_write, yield_context{to_asio{}});
			break;

		case ready::READ:
			sd.async_wait(wait_type::wait_read, yield_context{to_asio{}});
			break;

		default:
			throw ircd::not_implemented{};
	}
}

/// Asynchronous callback when the socket is ready
///
/// This function calls back the handler when the socket is ready
/// for the operation of the specified type.
///
void
ircd::net::socket::wait(const wait_opts &opts,
                        wait_callback_ec callback)
{
	auto handle
	{
		std::bind(&socket::handle_ready, this, weak_from(*this), opts.type, std::move(callback), ph::_1)
	};

	set_timeout(opts.timeout);
	const unwind::exceptional unset{[this]
	{
		cancel_timeout();
	}};

	switch(opts.type)
	{
		case ready::ERROR:
			sd.async_wait(wait_type::wait_error, std::move(handle));
			break;

		case ready::WRITE:
			sd.async_wait(wait_type::wait_write, std::move(handle));
			break;

		case ready::READ:
		{
			static char buf[1] alignas(16);
			static const ilist<mutable_buffer> bufs{buf};
			__builtin_prefetch(buf, 1, 0); // 1 = write, 0 = no cache

			// The problem here is that waiting on the sd doesn't account for bytes
			// read into SSL that we didn't consume yet. If something is stuck in
			// those userspace buffers, the socket won't know about it and perform
			// the wait. ASIO should fix this by adding a ssl::stream.wait() method
			// which will bail out immediately in this case before passing up to the
			// real socket wait.
			if(SSL_peek(ssl.native_handle(), buf, sizeof(buf)) > 0)
			{
				handle(error_code{});
				break;
			}

			// The problem here is that the wait operation gives ec=success on both a
			// socket error and when data is actually available. We then have to check
			// using a non-blocking peek in the handler. By doing it this way here we
			// just get the error in the handler's ec.
			sd.async_receive(bufs, sd.message_peek, std::move(handle));
			//sd.async_wait(wait_type::wait_read, std::move(handle));
			break;
		}

		default:
			throw ircd::not_implemented{};
	}
}

void
ircd::net::socket::handle_ready(const std::weak_ptr<socket> wp,
                                const net::ready type,
                                const ec_handler callback,
                                error_code ec)
noexcept try
{
	using namespace boost::system::errc;
	using boost::system::system_category;

	// After life_guard is constructed it is safe to use *this in this frame.
	const life_guard<socket> s{wp};

	log.debug("socket(%p) local[%s] remote[%s] ready %s %s available:%zu",
	          this,
	          string(local_ipport(*this)),
	          string(remote_ipport(*this)),
	          reflect(type),
	          string(ec),
	          available(*this));

	if(!timedout)
		cancel_timeout();

	if(ec.category() == system_category()) switch(ec.value())
	{
		// We expose a timeout condition to the user, but hide
		// other cancellations from invoking the callback.
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
catch(const boost::system::system_error &e)
{
	log.error("socket(%p) handle: %s",
	          this,
	          e.what());

	assert(0);
	call_user(callback, e.code());
}
catch(const std::bad_weak_ptr &e)
{
	// This handler may still be registered with asio after the socket destructs, so
	// the weak_ptr will indicate that fact. However, this is never intended and is
	// a debug assertion which should be corrected.
	log.warning("socket(%p) belated callback to handler... (%s)",
	            this,
	            e.what());

	assert(0);
	call_user(callback, ec);
}
catch(const std::exception &e)
{
	log.critical("socket(%p) handle: %s",
	             this,
	             e.what());

	assert(0);
	call_user(callback, ec);
}

void
ircd::net::socket::handle_timeout(const std::weak_ptr<socket> wp,
                                  ec_handler callback,
                                  const error_code &ec)
noexcept try
{
	using namespace boost::system::errc;

	switch(ec.value())
	{
		// A 'success' for this handler means there was a timeout on the socket
		case success: if(likely(!wp.expired()))
		{
			assert(timedout == false);
			timedout = true;
			sd.cancel();
			break;
		}
		else break;

		// A cancelation means there was no timeout.
		case operation_canceled: if(likely(!wp.expired()))
		{
			assert(ec.category() == boost::system::system_category());
			assert(timedout == false);
			timedout = false;
			break;
		}
		else break;

		// All other errors are unexpected, logged and ignored here.
		default:
		{
			log.critical("socket(%p) handle_timeout: unexpected: %s\n",
			             (const void *)this,
			             string(ec));
			assert(0);
			break;
		}
	}

	if(callback)
		call_user(callback, ec);
}
catch(const std::exception &e)
{
	log.critical("socket(%p) handle timeout: %s",
	             (const void *)this,
	             e.what());
	assert(0);
	if(callback)
		call_user(callback, ec);
}

void
ircd::net::socket::handle_connect(std::weak_ptr<socket> wp,
                                  const open_opts opts,
                                  eptr_handler callback,
                                  const error_code &ec)
noexcept try
{
	const life_guard<socket> s{wp};
	assert(!timedout || ec == boost::system::errc::operation_canceled);
	log.debug("socket(%p) local[%s] remote[%s] connect %s",
	          this,
	          string(local_ipport(*this)),
	          string(remote_ipport(*this)),
	          string(ec));

	// The timer was set by socket::connect() and may need to be canceled.
	if(!timedout)
		cancel_timeout();

	// A connect error; abort here by calling the user back with error.
	if(ec)
		return call_user(callback, ec);

	// Toggles the behavior of non-async functions; see func comment
	blocking(*this, false);

	// Try to set the user's socket options now; if something fails we can
	// invoke their callback with the error from the exception handler.
	if(opts.sopts)
		set(*this, *opts.sopts);

	// The user can opt out of performing the handshake here.
	if(!opts.handshake)
		return call_user(callback, ec);

	handshake(opts, std::move(callback));
}
catch(const std::bad_weak_ptr &e)
{
	log.warning("socket(%p) belated callback to handle_connect... (%s)",
	            this,
	            e.what());

	assert(0);
	call_user(callback, ec);
}
catch(const boost::system::system_error &e)
{
	log.error("socket(%p) after connect: %s",
	          this,
	          e.what());

	assert(0);
	call_user(callback, e.code());
}
catch(const std::exception &e)
{
	log.critical("socket(%p) handle_connect: %s",
	             this,
	             e.what());

	assert(0);
	call_user(callback, ec);
}

void
ircd::net::socket::handle_disconnect(std::shared_ptr<socket> s,
                                     eptr_handler callback,
                                     const error_code &ec)
noexcept try
{
	assert(!timedout || ec == boost::system::errc::operation_canceled);
	log.debug("socket(%p) local[%s] remote[%s] disconnect %s",
	          this,
	          string(local_ipport(*this)),
	          string(remote_ipport(*this)),
	          string(ec));

	// The timer was set by socket::disconnect() and may need to be canceled.
	if(!timedout)
		cancel_timeout();

	sd.close();
	call_user(callback, ec);
}
catch(const boost::system::system_error &e)
{
	log.error("socket(%p) disconnect: %s",
	          this,
	          e.what());

	assert(0);
	call_user(callback, e.code());
}
catch(const std::exception &e)
{
	log.critical("socket(%p) disconnect: %s",
	             this,
	             e.what());

	assert(0);
	call_user(callback, ec);
}

void
ircd::net::socket::handle_handshake(std::weak_ptr<socket> wp,
                                    eptr_handler callback,
                                    const error_code &ec)
noexcept try
{
	const life_guard<socket> s{wp};
	assert(!timedout || ec == boost::system::errc::operation_canceled);
	log.debug("socket(%p) local[%s] remote[%s] handshake %s",
	          this,
	          string(local_ipport(*this)),
	          string(remote_ipport(*this)),
	          string(ec));

	// The timer was set by socket::handshake() and may need to be canceled.
	if(!timedout)
		cancel_timeout();

	// This is the end of the asynchronous call chain; the user is called
	// back with or without error here.
	call_user(callback, ec);
}
catch(const boost::system::system_error &e)
{
	log.error("socket(%p) after handshake: %s",
	          this,
	          e.what());

	assert(0);
	call_user(callback, e.code());
}
catch(const std::bad_weak_ptr &e)
{
	log.warning("socket(%p) belated callback to handle_handshake... (%s)",
	            this,
	            e.what());
	assert(0);
	call_user(callback, ec);
}
catch(const std::exception &e)
{
	log.critical("socket(%p) handle_handshake: %s",
	             this,
	             e.what());
	assert(0);
	call_user(callback, ec);
}

bool
ircd::net::socket::handle_verify(const bool valid,
                                 asio::ssl::verify_context &vc,
                                 const open_opts &opts)
noexcept try
{
	// `valid` indicates whether or not there's an anomaly with the
	// certificate; if so, it is usually enumerated by the `switch()`
	// statement below. If `valid` is false, this function can return
	// true to continue but it appears this function will be called a
	// second time with `valid=true`.
	//
	// TODO: XXX: This behavior must be confirmed since we return true
	// TODO: XXX: early on recoverable errors and skip other checks
	// TODO: XXX: expecting a second call..
	//

	// The user can set this option to bypass verification.
	if(!opts.verify_certificate)
		return true;

	// X509_STORE_CTX &
	assert(vc.native_handle());
	const auto &stctx{*vc.native_handle()};
	const auto &cert{openssl::current_cert(stctx)};
	const auto reject{[&stctx, &opts]
	{
		throw inauthentic
		{
			"%s #%ld: %s",
			common_name(opts),
			openssl::get_error(stctx),
			openssl::get_error_string(stctx)
		};
	}};

	if(!valid)
	{
		thread_local char buf[4_KiB];
		const critical_assertion ca;
		log.warning("verify[%s]: %s :%s",
		            common_name(opts),
		            openssl::get_error_string(stctx),
		            openssl::print_subject(buf, cert));
	}

	if(!valid) switch(openssl::get_error(stctx))
	{
		case X509_V_OK:
			assert(0);

		default:
			reject();
			break;

		case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
			assert(openssl::get_error_depth(stctx) == 0);
			if(opts.allow_self_signed)
				return true;

			reject();
			break;

		case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
			if(opts.allow_self_chain)
				return true;

			reject();
			break;
	}

	if(opts.verify_common_name)
	{
		if(unlikely(empty(common_name(opts))))
			throw inauthentic
			{
				"No common name specified in connection options"
			};

		//TODO: this object makes an std::string
		boost::asio::ssl::rfc2818_verification verifier
		{
			std::string(common_name(opts))
		};

		if(!verifier(true, vc))
		{
			thread_local char buf[256];
			const critical_assertion ca;
			throw inauthentic
			{
				"/CN=%s does not match target host %s :%s",
				openssl::subject_common_name(buf, cert),
				common_name(opts),
				openssl::get_error_string(stctx)
			};
		}
	}

	{
		thread_local char buf[4_KiB];
		const critical_assertion ca;
		log.debug("verify[%s]: %s",
		          common_name(opts),
		          openssl::print_subject(buf, cert));
	}

	return true;
}
catch(const inauthentic &e)
{
	log.error("Certificate rejected: %s", e.what());
	return false;
}
catch(const std::exception &e)
{
	log.critical("Certificate error: %s", e.what());
	return false;
}

void
ircd::net::socket::call_user(const ec_handler &callback,
                             const error_code &ec)
noexcept try
{
	callback(ec);
}
catch(const std::exception &e)
{
	log.critical("socket(%p) async handler: unhandled exception: %s",
	             this,
	             e.what());
}

void
ircd::net::socket::call_user(const eptr_handler &callback,
                             const error_code &ec)
noexcept try
{
	callback(make_eptr(ec));
}
catch(const std::exception &e)
{
	log.critical("socket(%p) async handler: unhandled exception: %s",
	             this,
	             e.what());
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
	set_timeout(t, nullptr);
}

void
ircd::net::socket::set_timeout(const milliseconds &t,
                               ec_handler callback)
{
	cancel_timeout();
	timedout = false;
	if(t < milliseconds(0))
		return;

	auto handler
	{
		std::bind(&socket::handle_timeout, this, weak_from(*this), std::move(callback), ph::_1)
	};

	timer.expires_from_now(t);
	timer.async_wait(std::move(handler));
}

bool
ircd::net::socket::has_timeout()
const noexcept
{
	return !timedout && timer.expires_from_now() != milliseconds{0};
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
                                                socket::ec_handler handler)
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

std::ostream &
ircd::net::operator<<(std::ostream &s, const remote &t)
{
	thread_local char buf[256];
	const critical_assertion ca;
	s << string(buf, t);
	return s;
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
	else
	{
		const auto len{fmt::sprintf(buf, "%s => %s", remote.hostname, string(ipp))};
		return { data(buf), size_t(len) };
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// net/ipport.h
//

std::ostream &
ircd::net::operator<<(std::ostream &s, const ipport &t)
{
	thread_local char buf[256];
	const critical_assertion ca;
	s << string(buf, t);
	return s;
}

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

//
// ipport
//

ircd::net::ipport::ipport(const string_view &ip,
                          const string_view &port)
:ipport
{
	ip, lex_cast<uint16_t>(port)
}
{
}

ircd::net::ipport::ipport(const string_view &ip,
                          const uint16_t &port)
:ipport
{
	asio::ip::make_address(ip), port
}
{
}

ircd::net::ipport::ipport(const boost::asio::ip::address &address,
                          const uint16_t &port)
{
	std::get<TYPE>(*this) = address.is_v6();
	std::get<PORT>(*this) = port;

	if(is_v6(*this))
	{
		std::get<IP>(*this) = address.to_v6().to_bytes();
		std::reverse(std::get<IP>(*this).begin(), std::get<IP>(*this).end());
	}
	else host4(*this) = address.to_v4().to_ulong();
}

///////////////////////////////////////////////////////////////////////////////
//
// net/hostport.h
//

std::ostream &
ircd::net::operator<<(std::ostream &s, const hostport &t)
{
	thread_local char buf[256];
	const critical_assertion ca;
	s << string(buf, t);
	return s;
}

ircd::string_view
ircd::net::string(const mutable_buffer &buf,
                  const hostport &hp)
{
	const auto len
	{
		fmt::sprintf
		{
			buf, "%s:%u", host(hp), port(hp)
		}
	};

	return { data(buf), size_t(len) };
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
	std::string ret(128, char{});
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
