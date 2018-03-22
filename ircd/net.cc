// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/asio.h>

namespace ircd::net
{
	ctx::dock dock;

	void wait_close_sockets();
}

void
ircd::net::wait_close_sockets()
{
    while(socket::instances)
		if(dock.wait_for(seconds(2)) != ctx::cv_status::no_timeout)
			log.warning("Waiting for %zu sockets to destruct",
			            socket::instances);
}

///////////////////////////////////////////////////////////////////////////////
//
// init
//

/// Network subsystem initialization
ircd::net::init::init()
{
	assert(ircd::ios);
	assert(!net::dns::resolver);
	net::dns::resolver = new struct dns::resolver();

	sslv23_client.set_verify_mode(asio::ssl::verify_peer);
	sslv23_client.set_default_verify_paths();
}

/// Network subsystem shutdown
ircd::net::init::~init()
{
	wait_close_sockets();
	delete net::dns::resolver;
	net::dns::resolver = nullptr;
}

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

ircd::const_buffer
ircd::net::peer_cert_der(const mutable_buffer &buf,
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
ircd::net::opened(const socket &socket)
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

/// Yields ircd::ctx until at least some buffers are sent.
///
/// This is blocking behavior; use this if the following are true:
///
/// * You put a timer on the socket so if the remote slows us down the data
/// will not occupy the daemon's memory for a long time.
///
/// * You are willing to dedicate the ircd::ctx to sending the data to
/// the remote. The ircd::ctx will be yielding until the kernel has at least
/// some space to consume at least something from the supplied buffers.
///
size_t
ircd::net::write_few(socket &socket,
                     const vector_view<const const_buffer> &buffers)
{
	return socket.write_few(buffers);
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

/// Non-blocking discard of up to len bytes. The amount of bytes discarded
/// is returned. Zero is only returned if len==0 because the EAGAIN is
/// thrown. If any bytes have been discarded any EAGAIN encountered in
/// this function's internal loop is not thrown, but used to exit the loop.
///
size_t
ircd::net::discard_any(socket &socket,
                       const size_t &len)
{
	static char buffer[512] alignas(16);

	size_t remain{len}; while(remain) try
	{
		const mutable_buffer mb
		{
			buffer, std::min(remain, sizeof(buffer))
		};

		__builtin_prefetch(data(mb), 1, 0);    // 1 = write, 0 = no cache
		remain -= read_one(socket, mb);
	}
	catch(const boost::system::system_error &e)
	{
		if(e.code() == boost::system::errc::resource_unavailable_try_again)
			if(remain <= len)
				break;

		throw;
	}

	return len - remain;
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
ircd::net::read_few(socket &socket,
                    const vector_view<const mutable_buffer> &buffers)
{
	return socket.read_few(buffers);
}

/// Reads as much as possible. Non-blocking behavior.
///
/// This is intended for lowest-level/custom control and not preferred by
/// default for most users on an ircd::ctx.
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
/// default for most users on an ircd::ctx.
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
ircd::net::error_code
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
		if(eptr && !s->fini)
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
		dns(opts.hostport, std::move(connector));
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

///////////////////////////////////////////////////////////////////////////////
//
// net/acceptor.h
//

ircd::log::log
ircd::net::listener::acceptor::log
{
	"listener"
};

decltype(ircd::net::listener::acceptor::timeout)
ircd::net::listener::acceptor::timeout
{
	{ "name",     "ircd.net.acceptor.timeout" },
	{ "default",  5000L                       },
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
	throw assertive
	{
		"%s: %s", std::string(*this), e.what()
	};
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
	const unwind::exceptional drop{[&sock]
	{
		if(!bool(sock))
			return;

		error_code ec;
		sock->sd.close(ec);
	}};

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
	sock->set_timeout(milliseconds(timeout));
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
catch(const boost::system::system_error &e)
{
	using namespace boost::system::errc;

	const auto &fac{e.code() == timed_out? log::DEBUG : log::ERROR};
	log(fac, "%s: socket(%p) in handshake(): %s",
	          std::string(*this),
	          sock.get(),
	          string(e));
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
	using boost::system::system_error;
	using boost::system::system_category;
	using namespace boost::system::errc;

	if(unlikely(interrupting))
		throw ctx::interrupted();

	if(likely(ec.category() == system_category())) switch(ec.value())
	{
		case success:
			return;

		case operation_canceled:
			if(sock.timedout)
				throw system_error(timed_out, system_category());
			else
				break;

		default:
			break;
	}

	throw system_error(ec);
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

ircd::net::listener::acceptor::operator
std::string()
const
{
	return fmt::snstringf
	{
		256, "'%s' @ [%s]:%u",
		name,
		string(ep.address()),
		ep.port()
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

decltype(ircd::net::socket::count)
ircd::net::socket::count
{};

decltype(ircd::net::socket::instances)
ircd::net::socket::instances
{};

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
	++count;
	++instances;
}

/// The dtor asserts that the socket is not open/connected requiring a
/// an SSL close_notify. There's no more room for async callbacks via
/// shared_ptr after this dtor.
ircd::net::socket::~socket()
noexcept try
{
	if(unlikely(--instances == 0))
		net::dock.notify_all();

	if(unlikely(RB_DEBUG_LEVEL && opened(*this)))
		throw assertive
		{
			"Failed to ensure socket(%p) is disconnected from %s before dtor.",
			this,
			string(remote())
		};
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

	assert(!fini);
	fini = true;
	cancel();

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
	throw assertive
	{
		"socket(%p) disconnect: type: %d: %s",
		(const void *)this,
		uint(opts.type),
		e.what()
	};
}

void
ircd::net::socket::cancel()
noexcept
{
	cancel_timeout();
	boost::system::error_code ec;
	sd.cancel(ec);
	assert(!ec);
}

void
ircd::net::socket::wait(const wait_opts &opts,
                        wait_callback_eptr callback)
{
	wait(opts, [callback(std::move(callback))]
	(const error_code &ec)
	{
		if(likely(!ec))
			return callback(std::exception_ptr{});

		using boost::system::system_error;
		callback(std::make_exception_ptr(system_error{ec}));
	});
}

/// Asynchronous callback when the socket is ready
///
/// Overload for operator() without a timeout. see: operator()
///
void
ircd::net::socket::wait(const wait_opts &opts)
try
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
catch(const boost::system::system_error &e)
{
	using namespace boost::system::errc;
	using boost::system::system_category;

	if(e.code() == operation_canceled && timedout)
		throw boost::system::system_error
		{
			timed_out, system_category()
		};

	throw;
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
	set_timeout(opts.timeout);
	const unwind::exceptional unset{[this]
	{
		cancel_timeout();
	}};

	switch(opts.type)
	{
		case ready::ERROR:
		{
			auto handle
			{
				std::bind(&socket::handle_ready, this, weak_from(*this), opts.type, std::move(callback), ph::_1, 0UL)
			};

			sd.async_wait(wait_type::wait_error, std::move(handle));
			break;
		}

		case ready::WRITE:
		{
			auto handle
			{
				std::bind(&socket::handle_ready, this, weak_from(*this), opts.type, std::move(callback), ph::_1, 0UL)
			};

			sd.async_wait(wait_type::wait_write, std::move(handle));
			break;
		}

		case ready::READ:
		{
			static char buf[1] alignas(16);
			static const ilist<mutable_buffer> bufs{buf};
			__builtin_prefetch(buf, 1, 0); // 1 = write, 0 = no cache

			auto handle
			{
				std::bind(&socket::handle_ready, this, weak_from(*this), opts.type, std::move(callback), ph::_1, ph::_2)
			};

			// The problem here is that waiting on the sd doesn't account for bytes
			// read into SSL that we didn't consume yet. If something is stuck in
			// those userspace buffers, the socket won't know about it and perform
			// the wait. ASIO should fix this by adding a ssl::stream.wait() method
			// which will bail out immediately in this case before passing up to the
			// real socket wait.
			if(SSL_peek(ssl.native_handle(), buf, sizeof(buf)) >= ssize_t(sizeof(buf)))
			{
				ircd::post([handle(std::move(handle))]
				{
					handle(error_code{}, 1UL);
				});

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
                                error_code ec,
                                const size_t bytes)
noexcept try
{
	using namespace boost::system::errc;
	using boost::system::system_category;

	// After life_guard is constructed it is safe to use *this in this frame.
	const life_guard<socket> s{wp};

	if(!timedout && ec != operation_canceled && !fini)
		cancel_timeout();

	if(timedout && ec == operation_canceled && ec.category() == system_category())
		ec = { timed_out, system_category() };

	if(unlikely(!ec && !sd.is_open()))
		ec = { bad_file_descriptor, system_category() };

	if(type == ready::READ && !ec && bytes == 0)
		ec = { asio::error::eof, asio::error::get_misc_category() };

	log.debug("socket(%p) local[%s] remote[%s] ready %s %s avail:%zu:%zu:%d",
	          this,
	          string(local_ipport(*this)),
	          string(remote_ipport(*this)),
	          reflect(type),
	          string(ec),
	          type == ready::READ? bytes : 0UL,
	          type == ready::READ? available(*this) : 0UL,
	          SSL_pending(ssl.native_handle()));

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
                                  error_code ec)
noexcept try
{
	using namespace boost::system::errc;
	using boost::system::system_category;

	if(unlikely(wp.expired()))
		return;

	switch(ec.value())
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
			assert(ec.category() == system_category());
			assert(timedout == false);
			break;
		}

		// All other errors are unexpected, logged and ignored here.
		default: throw assertive
		{
			"unexpected: %s\n",
			(const void *)this,
			string(ec)
		};
	}

	if(callback)
		call_user(callback, ec);
}
catch(const boost::system::system_error &e)
{
	using namespace boost::system::errc;
	using boost::system::system_category;

	const error_code &_ec{e.code()};
	switch(_ec.value())
	{
		case bad_file_descriptor:
			assert(ec.category() == system_category());
			if(fini)
				break;

		default:
			assert(0);
			log.critical("socket(%p) handle timeout: %s",
			             (const void *)this,
			             string(e));
			break;
	}

	if(callback)
		call_user(callback, _ec);
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
                                  error_code ec)
noexcept try
{
	using namespace boost::system::errc;
	using boost::system::system_category;

	const life_guard<socket> s{wp};
	log.debug("socket(%p) local[%s] remote[%s] connect %s",
	          this,
	          string(local_ipport(*this)),
	          string(remote_ipport(*this)),
	          string(ec));

	// The timer was set by socket::connect() and may need to be canceled.
	if(!timedout && ec != operation_canceled && !fini)
		cancel_timeout();

	if(timedout && ec == operation_canceled && ec.category() == system_category())
		ec = { timed_out, system_category() };

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
catch(const boost::system::system_error &e)
{
	log.error("socket(%p) after connect: %s",
	          this,
	          e.what());

	assert(0);
	call_user(callback, e.code());
}
catch(const std::bad_weak_ptr &e)
{
	log.warning("socket(%p) belated callback to handle_connect... (%s)",
	            this,
	            e.what());

	assert(0);
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
                                     error_code ec)
noexcept try
{
	using namespace boost::system::errc;
	using boost::system::system_category;

	assert(fini);
	if(!timedout && ec != operation_canceled)
		cancel_timeout();

	if(timedout && ec == operation_canceled && ec.category() == system_category())
		ec = { timed_out, system_category() };

	log.debug("socket(%p) local[%s] remote[%s] disconnect %s",
	          this,
	          string(local_ipport(*this)),
	          string(remote_ipport(*this)),
	          string(ec));

	// This ignores EOF and turns it into a success to alleviate user concern.
	if(ec.category() == asio::error::get_misc_category())
		if(ec.value() == asio::error::eof)
			ec = error_code{};

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
                                    error_code ec)
noexcept try
{
	using namespace boost::system::errc;
	using boost::system::system_category;

	const life_guard<socket> s{wp};

	if(!timedout && ec != operation_canceled && !fini)
		cancel_timeout();

	if(timedout && ec == operation_canceled && ec.category() == system_category())
		ec = { timed_out, system_category() };

	log.debug("socket(%p) local[%s] remote[%s] handshake %s",
	          this,
	          string(local_ipport(*this)),
	          string(remote_ipport(*this)),
	          string(ec));

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

	const auto err
	{
		openssl::get_error(stctx)
	};

	if(!valid) switch(err)
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

		case X509_V_ERR_CERT_HAS_EXPIRED:
			if(opts.allow_expired)
				return true;

			reject();
			break;
	}

	const bool verify_common_name
	{
		opts.verify_common_name &&
		(opts.verify_self_signed_common_name && err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT)
	};

	if(verify_common_name)
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

	close(*this, dc::RST, close_ignore);
}

void
ircd::net::socket::call_user(const eptr_handler &callback,
                             const error_code &ec)
noexcept try
{
	if(likely(!ec))
		return callback(std::exception_ptr{});

	using boost::system::system_error;
	callback(std::make_exception_ptr(system_error{ec}));
}
catch(const std::exception &e)
{
	log.critical("socket(%p) async handler: unhandled exception: %s",
	             this,
	             e.what());
}

ircd::milliseconds
ircd::net::socket::cancel_timeout()
noexcept
{
	const auto exp
	{
		timer.expires_from_now()
	};

	const auto ret
	{
		duration_cast<milliseconds>(exp)
	};

	timedout = false;
	boost::system::error_code ec;
	timer.cancel(ec);
	assert(!ec);
	assert(timedout == false);
	return ret;
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
	if(t < milliseconds(0))
		return;

	auto handler
	{
		std::bind(&socket::handle_timeout, this, weak_from(*this), std::move(callback), ph::_1)
	};

	timer.expires_from_now(t);
	timer.async_wait(std::move(handler));
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
// net/dns.h
//

/// Singleton instance of the public interface ircd::net::resolve
decltype(ircd::net::dns)
ircd::net::dns
{};

/// Singleton instance of the DNS cache
decltype(ircd::net::dns::cache)
ircd::net::dns::cache
{};

/// Singleton instance of the internal boost resolver wrapper.
decltype(ircd::net::dns::resolver)
ircd::net::dns::resolver
{};

/// Linkage for default opts
decltype(ircd::net::dns::opts_default)
ircd::net::dns::opts_default
{};

decltype(ircd::net::dns::cache::clear_nxdomain)
ircd::net::dns::cache::clear_nxdomain
{
	{ "name",     "ircd.net.dns.cache.clear_nxdomain" },
	{ "default",   43200L                             },
};

/// Convenience composition with a single ipport callback. This is the result of
/// an automatic chain of queries such as SRV and A/AAAA based on the input and
/// intermediate results.
void
ircd::net::dns::operator()(const hostport &hostport,
                           const opts &opts,
                           callback_ipport_one callback)
{
	//TODO: ip6
	auto calluser{[callback(std::move(callback))]
	(std::exception_ptr eptr, const uint32_t &ip, const uint16_t &port)
	{
		if(eptr)
			return callback(std::move(eptr), {});

		if(!ip)
			return callback(std::make_exception_ptr(net::not_found{"Host has no A record"}), {});

		const ipport ipport{ip, port};
		callback(std::move(eptr), ipport);
	}};

	if(!hostport.service)
		return operator()(hostport, opts, [hostport, calluser(std::move(calluser))]
		(std::exception_ptr eptr, const rfc1035::record::A &record)
		{
			calluser(std::move(eptr), record.ip4, port(hostport));
		});

	operator()(hostport, opts, [this, hostport(hostport), opts(opts), calluser(std::move(calluser))]
	(std::exception_ptr eptr, const rfc1035::record::SRV &record)
	mutable
	{
		//TODO: we get NXDOMAIN and it kills the chain..
		//if(eptr)
		//	return callback(std::move(eptr), {});

		if(!record.tgt.empty())
			host(hostport) = record.tgt;

		if(record.port != 0)
			port(hostport) = record.port;

		// Have to kill the service name to not run another SRV query now.
		hostport.service = {};
		opts.srv = {};
		this->operator()(hostport, opts, [hostport, calluser(std::move(calluser))]
		(std::exception_ptr eptr, const rfc1035::record::A &record)
		{
			calluser(std::move(eptr), record.ip4, port(hostport));
		});
	});
}

/// Convenience callback with a single SRV record which was selected from
/// the vector with stochastic respect for weighting and priority.
void
ircd::net::dns::operator()(const hostport &hostport,
                           const opts &opts,
                           callback_SRV_one callback)
{
	assert(bool(ircd::net::dns::resolver));
	operator()(hostport, opts, [callback(std::move(callback))]
	(std::exception_ptr eptr, const vector_view<const rfc1035::record *> rrs)
	{
		if(eptr)
			return callback(std::move(eptr), {});

		//TODO: prng on weight / prio plz
		for(size_t i(0); i < rrs.size(); ++i)
		{
			const auto &rr{*rrs.at(i)};
			if(rr.type != 33)
				continue;

			const auto &record(rr.as<const rfc1035::record::SRV>());
			return callback(std::move(eptr), record);
		}

		return callback(std::move(eptr), {});
	});
}

/// Convenience callback with a single A record which was selected from
/// the vector randomly.
void
ircd::net::dns::operator()(const hostport &hostport,
                           const opts &opts,
                           callback_A_one callback)
{
	assert(bool(ircd::net::dns::resolver));
	operator()(hostport, opts, [callback(std::move(callback))]
	(std::exception_ptr eptr, const vector_view<const rfc1035::record *> rrs)
	{
		if(eptr)
			return callback(std::move(eptr), {});

		//TODO: prng plz
		for(size_t i(0); i < rrs.size(); ++i)
		{
			const auto &rr{*rrs.at(i)};
			if(rr.type != 1)
				continue;

			const auto &record(rr.as<const rfc1035::record::A>());
			return callback(std::move(eptr), record);
		}

		return callback(std::move(eptr), {});
	});
}

/// Fundamental callback with a vector of abstract resource records.
void
ircd::net::dns::operator()(const hostport &hostport,
                           const opts &opts,
                           callback cb)
{
	if(opts.cache_check)
		if(query_cache(hostport, opts, cb))
			return;

	assert(bool(ircd::net::dns::resolver));
	(*resolver)(hostport, opts, std::move(cb));
}

/// This function has an opportunity to respond from the DNS cache. If it
/// returns true, that indicates it responded by calling back the user and
/// nothing further should be done for them. If it returns false, that
/// indicates it did not respond and to proceed normally. The response can
/// be of a cached successful result, or a cached error. Both will return
/// true.
///
bool
ircd::net::dns::query_cache(const hostport &hp,
                            const opts &opts,
                            const callback &cb)
{
	thread_local const rfc1035::record *record[resolver::MAX_COUNT];
	std::exception_ptr eptr;
	size_t count{0};

	//TODO: Better deduction
	if(hp.service || opts.srv) // deduced SRV query
	{
		thread_local char srvbuf[512];
		const string_view srvhost
		{
			make_SRV_key(srvbuf, hp, opts)
		};

		auto &map{cache.SRV};
		const auto pit{map.equal_range(srvhost)};
		if(pit.first == pit.second)
			return false;

		const auto &now{ircd::time()};
		for(auto it(pit.first); it != pit.second; )
		{
			const auto &rr{it->second};

			// Cached entry is too old, ignore and erase
			if(rr.ttl < now)
			{
				it = map.erase(it);
				continue;
			}

			// Cached entry is a cached error, we set the eptr, but also
			// include the record and increment the count like normal.
			assert(!eptr);
			if(!rr.tgt)
			{
				const auto rcode{3}; //NXDomain
				eptr = std::make_exception_ptr(rfc1035::error
				{
					"protocol error #%u (cached) :%s", rcode, rfc1035::rcode.at(rcode)
				});
			}

			record[count++] = &rr;
			++it;
		}
	}
	else // Deduced A query (for now)
	{
		auto &map{cache.A};
		const auto pit{map.equal_range(std::string{host(hp)})}; //TODO: XXX
		if(pit.first == pit.second)
			return false;

		const auto &now{ircd::time()};
		for(auto it(pit.first); it != pit.second; )
		{
			const auto &rr{it->second};

			// Cached entry is too old, ignore and erase
			if(rr.ttl < now)
			{
				it = map.erase(it);
				continue;
			}

			// Cached entry is a cached error, we set the eptr, but also
			// include the record and increment the count like normal.
			assert(!eptr);
			if(!rr.ip4)
			{
				const auto rcode{3}; //NXDomain
				eptr = std::make_exception_ptr(rfc1035::error
				{
					"protocol error #%u (cached) :%s", rcode, rfc1035::rcode.at(rcode)
				});
			}

			record[count++] = &rr;
			++it;
		}
	}

	assert(count || !eptr);        // no error if no cache response
	assert(!eptr || count == 1);   // if error, should only be one entry.

	if(count)
		cb(std::move(eptr), vector_view<const rfc1035::record *>(record, count));

	return count;
}

ircd::string_view
ircd::net::dns::make_SRV_key(const mutable_buffer &out,
                             const hostport &hp,
                             const opts &opts)
{
	if(!opts.srv)
		return fmt::sprintf
		{
			out, "_%s._%s.%s", service(hp), opts.proto, host(hp)
		};
	else
		return fmt::sprintf
		{
			out, "%s%s", opts.srv, host(hp)
		};
}

///////////////////////////////////////////////////////////////////////////////
//
// net/resolver.h
//

decltype(ircd::net::dns::resolver::timeout)
ircd::net::dns::resolver::timeout
{
	{ "name",     "ircd.net.dns.resolver.timeout" },
	{ "default",   10000L                         },
};

decltype(ircd::net::dns::resolver::send_rate)
ircd::net::dns::resolver::send_rate
{
	{ "name",     "ircd.net.dns.resolver.send_rate" },
	{ "default",   20L                              },
};

ircd::net::dns::resolver::resolver()
:ns{*ircd::ios}
,timeout_context
{
	"dnsres T", 64_KiB, std::bind(&resolver::timeout_worker, this), context::POST
}
,sendq_context
{
	"dnsres S", 64_KiB, std::bind(&resolver::sendq_worker, this), context::POST
}
{
	ns.open(ip::udp::v4());
	ns.non_blocking(true);
	set_handle();
	init_servers();
}

ircd::net::dns::resolver::~resolver()
noexcept
{
	ns.close();
	sendq_context.interrupt();
	timeout_context.interrupt();
	assert(tags.empty());
}

void
ircd::net::dns::resolver::sendq_worker()
try
{
	while(1)
	{
		assert(sendq.empty() || !tags.empty());
		dock.wait([this]
		{
			return !sendq.empty();
		});

		assert(sendq.size() < 65535);
		assert(sendq.size() <= tags.size());
		ctx::sleep(milliseconds(send_rate));
		assert(!sendq.empty());
		const auto &next(sendq.front());
		const unwind pop{[this]
		{
			sendq.pop_front();
		}};

		flush(next);
	}
}
catch(const ctx::interrupted &)
{
	return;
}

void
ircd::net::dns::resolver::flush(const queued &next)
try
{
	auto &tag{tags.at(next.first)};
	const const_buffer buf
	{
		data(next.second), size(next.second)
	};

	send_query(buf, tag);
}
catch(const std::out_of_range &e)
{
	log::error
	{
		"Queued tag id[%u] is no longer mapped", next.first
	};
}

void
ircd::net::dns::resolver::timeout_worker()
try
{
	while(1)
	{
		dock.wait([this]
		{
			return !tags.empty();
		});

		ctx::sleep(milliseconds(timeout));
		check_timeouts(milliseconds(timeout));
	}
}
catch(const ctx::interrupted &)
{
	return;
}

void
ircd::net::dns::resolver::check_timeouts(const milliseconds &timeout)
{
	const auto cutoff
	{
		now<steady_point>() - timeout
	};

	auto it(begin(tags));
	while(it != end(tags))
	{
		const auto &id(it->first);
		auto &tag(it->second);
		if(!check_timeout(id, tag, cutoff))
			it = tags.erase(it);
		else
			++it;
	}
}

bool
ircd::net::dns::resolver::check_timeout(const uint16_t &id,
                                        tag &tag,
                                        const steady_point &cutoff)
{
	if(tag.last == steady_point{})
		return true;

	if(tag.last < cutoff)
		return true;

	//TODO: retry
	log.error("DNS timeout id:%u", id);

	const error_code ec
	{
		boost::system::errc::timed_out, boost::system::system_category()
	};

	if(tag.cb)
	{
		using boost::system::system_error;
		tag.cb(std::make_exception_ptr(system_error{ec}), {});
	}

	return false;
}

/// Internal resolver entry interface.
void
ircd::net::dns::resolver::operator()(const hostport &hostport,
                                     const opts &opts,
                                     callback callback)
{
	auto &tag
	{
		set_tag(resolver::tag
		{
			hostport, opts, std::move(callback)
		})
	};

	// Escape trunk
	const unwind::exceptional untag{[this, &tag]
	{
		tags.erase(tag.id);
	}};

	thread_local char buf[64_KiB];
	submit(make_query(buf, tag), tag);
}

ircd::const_buffer
ircd::net::dns::resolver::make_query(const mutable_buffer &buf,
                                     const tag &tag)
const
{
	//TODO: Better deduction
	if(tag.hp.service || tag.opts.srv)
	{
		thread_local char srvbuf[512];
		const string_view srvhost
		{
			make_SRV_key(srvbuf, tag.hp, tag.opts)
		};

		const rfc1035::question question{srvhost, "SRV"};
		return rfc1035::make_query(buf, tag.id, question);
	}

	const rfc1035::question question{host(tag.hp), "A"};
	return rfc1035::make_query(buf, tag.id, question);
}

ircd::net::dns::resolver::tag &
ircd::net::dns::resolver::set_tag(tag &&tag)
{
	while(tags.size() < 65535)
	{
		tag.id = ircd::rand::integer(1, 65535);
		auto it{tags.lower_bound(tag.id)};
		if(it != end(tags) && it->first == tag.id)
			continue;

		it = tags.emplace_hint(it, tag.id, std::move(tag));
		dock.notify_one();
		return it->second;
	}

	throw assertive
	{
		"Too many DNS queries"
	};
}

void
ircd::net::dns::resolver::submit(const const_buffer &buf,
                                 tag &tag)
{
	const auto rate(milliseconds(send_rate) / server.size());
	const auto elapsed(now<steady_point>() - send_last);
	if(elapsed >= rate)
		send_query(buf, tag);
	else
		queue_query(buf, tag);
}

void
ircd::net::dns::resolver::queue_query(const const_buffer &buf,
                                      tag &tag)
{
	sendq.emplace_back(tag.id, std::string(data(buf), size(buf)));
	dock.notify_one();
}

void
ircd::net::dns::resolver::send_query(const const_buffer &buf,
                                     tag &tag)
try
{
	assert(!server.empty());
	++server_next %= server.size();
	const auto &ep{server.at(server_next)};
	send_query(ep, buf, tag);
}
catch(const std::out_of_range &)
{
	throw error
	{
		"No DNS servers available for query"
	};
}

void
ircd::net::dns::resolver::send_query(const ip::udp::endpoint &ep,
                                     const const_buffer &buf,
                                     tag &tag)
{
	assert(ns.non_blocking());
	ns.send_to(asio::const_buffers_1(buf), ep);
	send_last = now<steady_point>();
	tag.last = send_last;
}

void
ircd::net::dns::resolver::set_handle()
{
	auto handler
	{
		std::bind(&resolver::handle, this, ph::_1, ph::_2)
	};

	const asio::mutable_buffers_1 bufs{reply, sizeof(reply)};
	ns.async_receive_from(bufs, reply_from, std::move(handler));
}

void
ircd::net::dns::resolver::handle(const error_code &ec,
                                 const size_t &bytes)
noexcept try
{
	if(!handle_error(ec))
		return;

	const unwind reset{[this]
	{
		set_handle();
	}};

	if(unlikely(bytes < sizeof(rfc1035::header)))
		throw rfc1035::error
		{
			"Got back %zu bytes < rfc1035 %zu byte header",
			bytes,
			sizeof(rfc1035::header)
		};

	char *const reply
	{
		this->reply
	};

	rfc1035::header &header
	{
		*reinterpret_cast<rfc1035::header *>(reply)
	};

	bswap(&header.qdcount);
	bswap(&header.ancount);
	bswap(&header.nscount);
	bswap(&header.arcount);

	const const_buffer body
	{
		reply + sizeof(header), bytes - sizeof(header)
	};

	handle_reply(header, body);
}
catch(const std::exception &e)
{
	throw assertive
	{
		"resolver::handle_reply(): %s", e.what()
	};
}

void
ircd::net::dns::resolver::handle_reply(const header &header,
                                       const const_buffer &body)
try
{
	const auto &id{header.id};
	const auto it{tags.find(id)};
	if(it == end(tags))
		throw error
		{
			"DNS reply from %s for unrecognized tag id:%u",
			string(reply_from),
			id
		};

	auto &tag{it->second};
	const unwind untag{[this, &it]
	{
		tags.erase(it);
	}};

	handle_reply(header, body, tag);
}
catch(const std::exception &e)
{
	log.error("%s", e.what());
	return;
}

void
ircd::net::dns::resolver::handle_reply(const header &header,
                                       const const_buffer &body,
                                       tag &tag)
try
{
	if(unlikely(header.qr != 1))
		throw rfc1035::error
		{
			"Response header is marked as 'Query' and not 'Response'"
		};

	if(header.qdcount > MAX_COUNT || header.ancount > MAX_COUNT)
		throw error
		{
			"Response contains too many sections..."
		};

	const_buffer buffer
	{
		body
	};

	// Questions are regurgitated back to us so they must be parsed first
	thread_local std::array<rfc1035::question, MAX_COUNT> qd;
	for(size_t i(0); i < header.qdcount; ++i)
		consume(buffer, size(qd.at(i).parse(buffer)));

	if(!handle_error(header, qd.at(0), tag.opts))
		throw rfc1035::error
		{
			"protocol error #%u :%s", header.rcode, rfc1035::rcode.at(header.rcode)
		};

	// Answers are parsed into this buffer
	thread_local std::array<rfc1035::answer, MAX_COUNT> an;
	for(size_t i(0); i < header.ancount; ++i)
		consume(buffer, size(an[i].parse(buffer)));

	if(tag.opts.cache_result)
	{
		// We convert all TTL values in the answers to absolute epoch time
		// indicating when they expire. This makes more sense for our caches.
		const auto &now{ircd::time()};
		for(size_t i(0); i < header.ancount; ++i)
			an[i].ttl = now + an[i].ttl;
	}

	// The callback to the user will be passed a vector_view of pointers
	// to this array. The actual record instances will either be located
	// in the cache map or placement-newed to the buffer below.
	thread_local const rfc1035::record *record[MAX_COUNT];

	// This will be where we place the record instances which are dynamically
	// laid out and sized types. 512 bytes is assumed as a soft maximum for
	// each RR instance.
	thread_local uint8_t recbuf[MAX_COUNT * 512];

	size_t i(0);
	uint8_t *pos{recbuf};
	for(; i < header.ancount; ++i) switch(an[i].qtype)
	{
		case 1: // A records are inserted into cache
		{
			if(!tag.opts.cache_result)
			{
				record[i] = new (pos) rfc1035::record::A(an[i]);
				pos += sizeof(rfc1035::record::A);
				continue;
			}

			const auto &name{qd.at(0).name};
			const auto &host{rstrip(name, '.')};
			const auto &it{cache.A.emplace(host, an[i])};
			record[i] = &it->second;
			continue;
		}

		case 5:
		{
			record[i] = new (pos) rfc1035::record::CNAME(an[i]);
			pos += sizeof(rfc1035::record::CNAME);
			continue;
		}

		case 33:
		{
			if(!tag.opts.cache_result)
			{
				record[i] = new (pos) rfc1035::record::SRV(an[i]);
				pos += sizeof(rfc1035::record::SRV);
				continue;
			}

			const auto &name{qd.at(0).name};
			const auto &host{rstrip(name, '.')};
			const auto it{cache.SRV.emplace(host, an[i])};
			record[i] = &it->second;
			continue;
		}

		default:
		{
			record[i] = new (pos) rfc1035::record(an[i]);
			pos += sizeof(rfc1035::record);
			continue;
		}
	}

	if(tag.cb)
		tag.cb({}, vector_view<const rfc1035::record *>(record, i));
}
catch(const std::exception &e)
{
	// There's no need to flash red to the log for NXDOMAIN which is
	// common in this system when probing SRV.
	if(unlikely(header.rcode != 3))
		log.error("resolver tag:%u [%s]: %s",
		          tag.id,
		          string(tag.hp),
		          e.what());

	if(tag.cb)
		tag.cb(std::current_exception(), {});
}

bool
ircd::net::dns::resolver::handle_error(const header &header,
                                       const rfc1035::question &q,
                                       const dns::opts &opts)
{
	switch(header.rcode)
	{
		case 0: // NoError; continue
			return true;

		case 3: // NXDomain; exception
		{
			if(opts.cache_result) switch(q.qtype)
			{
				case 1: // A
				{
					const auto &host{rstrip(q.name, '.')};
					rfc1035::record::A record;
					record.ttl = ircd::time() + seconds(cache::clear_nxdomain).count();
					cache.A.emplace(host, record);
					break;
				}

				case 33:
				{
					const auto &host{rstrip(q.name, '.')};
					rfc1035::record::SRV record;
					record.ttl = ircd::time() + seconds(cache::clear_nxdomain).count();
					cache.SRV.emplace(host, record);
					break;
				}
			}

			return false;
		}

		default: // Unhandled error; exception
			return false;
	}
}

bool
ircd::net::dns::resolver::handle_error(const error_code &ec)
const
{
	using namespace boost::system::errc;

	switch(ec.value())
	{
		case operation_canceled:
			return false;

		case success:
			return true;

		default:
			throw boost::system::system_error(ec);
	}
}

//TODO: x-platform
void
ircd::net::dns::resolver::init_servers()
{
	const auto resolve_conf
	{
		fs::read("/etc/resolv.conf")
	};

	tokens(resolve_conf, '\n', [this](const auto &line)
	{
		const auto kv(split(line, ' '));
		if(kv.first == "nameserver")
		{
			const ipport server{kv.second, 53};
			this->server.emplace_back(make_endpoint_udp(server));
			log.debug("Found nameserver %s from resolv.conf",
			          string(server));
		}
	});
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
ircd::net::make_ipport(const boost::asio::ip::udp::endpoint &ep)
{
	return ipport
	{
		ep.address(), ep.port()
	};
}

ircd::net::ipport
ircd::net::make_ipport(const boost::asio::ip::tcp::endpoint &ep)
{
	return ipport
	{
		ep.address(), ep.port()
	};
}

boost::asio::ip::udp::endpoint
ircd::net::make_endpoint_udp(const ipport &ipport)
{
	return
	{
		is_v6(ipport)? ip::udp::endpoint
		{
			asio::ip::address_v6 { std::get<ipport.IP>(ipport) }, port(ipport)
		}
		: ip::udp::endpoint
		{
			asio::ip::address_v4 { host4(ipport) }, port(ipport)
		},
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

ircd::net::ipport::ipport(const rfc1035::record::A &rr,
                          const uint16_t &port)
:ipport
{
	rr.ip4, port
}
{
}

ircd::net::ipport::ipport(const rfc1035::record::AAAA &rr,
                          const uint16_t &port)
:ipport
{
	rr.ip6, port
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

std::string
ircd::net::canonize(const hostport &hp,
                    const uint16_t &port)
{
	const size_t len
	{
		size(host(hp)) + 6 // optimistic ':' + portnum
	};

	return ircd::string(len, [&hp, &port]
	(const mutable_buffer &buf)
	{
		return canonize(buf, hp, port);
	});
}

ircd::string_view
ircd::net::canonize(const mutable_buffer &buf,
                    const hostport &hp,
                    const uint16_t &port)
{
	if(net::port(hp) == 0 || net::port(hp) == port)
		return fmt::sprintf
		{
			buf, "%s", host(hp)
		};

	return fmt::sprintf
	{
		buf, "%s:%u", host(hp), net::port(hp)
	};
}

ircd::string_view
ircd::net::string(const mutable_buffer &buf,
                  const hostport &hp)
{
	if(empty(service(hp)))
		return fmt::sprintf
		{
			buf, "%s:%u", host(hp), port(hp)
		};

	if(port(hp) == 0)
		return fmt::sprintf
		{
			buf, "%s (%s)", host(hp), service(hp)
		};

	return fmt::sprintf
	{
		buf, "%s:%u (%s)", host(hp), port(hp), service(hp)
	};
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
