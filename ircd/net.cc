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
		if(!dock.wait_for(seconds(2)))
			log::warning
			{
				log, "Waiting for %zu sockets to destruct", socket::instances
			};
}

///////////////////////////////////////////////////////////////////////////////
//
// init
//

/// Network subsystem initialization
ircd::net::init::init()
{
	sslv23_client.set_verify_mode(asio::ssl::verify_peer);
	sslv23_client.set_default_verify_paths();
}

/// Network subsystem shutdown
ircd::net::init::~init()
noexcept
{
	wait_close_sockets();
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

ircd::string_view
ircd::net::peer_cert_der_sha256_b64(const mutable_buffer &buf,
                                    const socket &socket)
{
	thread_local char shabuf[sha256::digest_size];

	const auto hash
	{
		peer_cert_der_sha256(shabuf, socket)
	};

	return b64encode_unpadded(buf, hash);
}

ircd::const_buffer
ircd::net::peer_cert_der_sha256(const mutable_buffer &buf,
                                const socket &socket)
{
	thread_local char derbuf[16384];

	sha256
	{
		buf, peer_cert_der(derbuf, socket)
	};

	return
	{
		data(buf), sha256::digest_size
	};
}

ircd::const_buffer
ircd::net::peer_cert_der(const mutable_buffer &buf,
                         const socket &socket)
{
	const SSL &ssl(socket);
	const X509 &cert
	{
		openssl::peer_cert(ssl)
	};

	return openssl::i2d(buf, cert);
}

std::pair<size_t, size_t>
ircd::net::calls(const socket &socket)
noexcept
{
	return
	{
		socket.in.calls, socket.out.calls
	};
}

std::pair<size_t, size_t>
ircd::net::bytes(const socket &socket)
noexcept
{
	return
	{
		socket.in.bytes, socket.out.bytes
	};
}

ircd::string_view
ircd::net::loghead(const socket &socket)
{
	thread_local char buf[512];
	return loghead(buf, socket);
}

ircd::string_view
ircd::net::loghead(const mutable_buffer &out,
                   const socket &socket)
{
	thread_local char buf[2][128];
	return fmt::sprintf
	{
		out, "socket:%lu local[%s] remote[%s]",
		id(socket),
		string(buf[0], local_ipport(socket)),
		string(buf[1], remote_ipport(socket)),
	};
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

const uint64_t &
ircd::net::id(const socket &socket)
{
	return socket.id;
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
	catch(const std::system_error &e)
	{
		if(e.code() == std::errc::resource_unavailable_try_again)
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
const ircd::net::wait_opts_default;

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
std::error_code
ircd::net::wait(nothrow_t,
                socket &socket,
                const wait_opts &wait_opts)
try
{
	wait(socket, wait_opts);
	return {};
}
catch(const std::system_error &e)
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

decltype(ircd::net::close_opts::default_timeout)
ircd::net::close_opts::default_timeout
{
	{ "name",     "ircd.net.close.timeout"  },
	{ "default",  7500L                     },
};

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

decltype(ircd::net::open_opts::default_connect_timeout)
ircd::net::open_opts::default_connect_timeout
{
	{ "name",     "ircd.net.open.connect_timeout"  },
	{ "default",  7500L                            },
};

decltype(ircd::net::open_opts::default_handshake_timeout)
ircd::net::open_opts::default_handshake_timeout
{
	{ "name",     "ircd.net.open.handshake_timeout"  },
	{ "default",  7500L                              },
};

decltype(ircd::net::open_opts::default_verify_certificate)
ircd::net::open_opts::default_verify_certificate
{
	{ "name",     "ircd.net.open.verify_certificate"  },
	{ "default",  true                                },
};

decltype(ircd::net::open_opts::default_allow_self_signed)
ircd::net::open_opts::default_allow_self_signed
{
	{ "name",     "ircd.net.open.allow_self_signed"  },
	{ "default",  false                              },
};

decltype(ircd::net::open_opts::default_allow_self_chain)
ircd::net::open_opts::default_allow_self_chain
{
	{ "name",     "ircd.net.open.allow_self_chain"  },
	{ "default",  false                             },
};

decltype(ircd::net::open_opts::default_allow_expired)
ircd::net::open_opts::default_allow_expired
{
	{ "name",     "ircd.net.open.allow_expired"  },
	{ "default",  false                          },
};

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
	(std::exception_ptr eptr, const hostport &hp, const ipport &ipport)
	{
		if(eptr)
			return complete(std::move(eptr));

		const auto ep{make_endpoint(ipport)};
		socket.connect(ep, opts, std::move(complete));
	}};

	if(!opts.ipport)
		dns::resolve(opts.hostport, std::move(connector));
	else
		connector({}, opts.hostport, opts.ipport);
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

/// Option to indicate if any listener sockets should be allowed to bind. If
/// false then no listeners should bind. This is only effective on startup
/// unless a conf item updated function is implemented here.
decltype(ircd::net::listen)
ircd::net::listen
{
	{ "name",     "ircd.net.listen" },
	{ "default",  true              },
	{ "persist",  false             },
};

//
// listener
//

std::ostream &
ircd::net::operator<<(std::ostream &s, const listener &a)
{
	s << *a.acceptor;
	return s;
}

//
// listener::listener
//

ircd::net::listener::listener(const string_view &name,
                              const std::string &opts,
                              callback cb,
                              proffer pcb)
:listener
{
	name, json::object{opts}, std::move(cb), std::move(pcb)
}
{
}

ircd::net::listener::listener(const string_view &name,
                              const json::object &opts,
                              callback cb,
                              proffer pcb)
:acceptor
{
	std::make_shared<struct acceptor>(*this, name, opts, std::move(cb), std::move(pcb))
}
{
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

bool
ircd::net::listener::start()
{
	return acceptor && !acceptor->handle_set?
		acceptor->set_handle():
		false;
}

ircd::string_view
ircd::net::listener::name()
const
{
	assert(acceptor);
	return acceptor->name;
}

ircd::net::listener::operator
ircd::json::object()
const
{
	assert(acceptor);
	return acceptor->opts;
}

//
// listener_udp
//

std::ostream &
ircd::net::operator<<(std::ostream &s, const listener_udp &a)
{
	s << *a.acceptor;
	return s;
}

//
// listener_udp::listener_udp
//

ircd::net::listener_udp::listener_udp(const string_view &name,
                                      const std::string &opts)
:listener_udp
{
	name, json::object{opts}
}
{
}

ircd::net::listener_udp::listener_udp(const string_view &name,
                                      const json::object &opts)
:acceptor
{
	std::make_unique<struct acceptor>(name, opts)
}
{
}

ircd::net::listener_udp::~listener_udp()
noexcept
{
	if(acceptor)
		acceptor->join();
}

ircd::net::listener_udp::datagram &
ircd::net::listener_udp::operator()(datagram &datagram)
{
	assert(acceptor);
	return acceptor->operator()(datagram);
}

ircd::string_view
ircd::net::listener_udp::name()
const
{
	assert(acceptor);
	return acceptor->name;
}

ircd::net::listener_udp::operator
ircd::json::object()
const
{
	assert(acceptor);
	return acceptor->opts;
}

///////////////////////////////////////////////////////////////////////////////
//
// net/acceptor.h
//

namespace ircd::net
{
	thread_local char logheadbuf[512];
}

//
// listener::acceptor
//

decltype(ircd::net::listener::acceptor::log)
ircd::net::listener::acceptor::log
{
	"listener"
};

decltype(ircd::net::listener::acceptor::timeout)
ircd::net::listener::acceptor::timeout
{
	{ "name",     "ircd.net.acceptor.timeout" },
	{ "default",  12000L                      },
};

std::ostream &
ircd::net::operator<<(std::ostream &s, const struct listener::acceptor &a)
{
	thread_local char addrbuf[128];
	s << "'" << a.name << "' @ [" << string(addrbuf, a.ep.address()) << "]:" << a.ep.port();
	return s;
}

//
// listener::acceptor::acceptor
//

ircd::net::listener::acceptor::acceptor(net::listener &listener,
                                        const string_view &name,
                                        const json::object &opts,
                                        listener::callback cb,
                                        listener::proffer pcb)
try
:listener
{
	&listener
}
,name
{
	name
}
,opts
{
	opts
}
,backlog
{
	//TODO: XXX
	//boost::asio::ip::tcp::socket::max_connections   <-- linkage failed?
	std::min(opts.get<uint>("backlog", SOMAXCONN), uint(SOMAXCONN))
}
,cb
{
	std::move(cb)
}
,pcb
{
	std::move(pcb)
}
,ssl
{
	asio::ssl::context::method::sslv23_server
}
,ep
{
	ip::address::from_string(unquote(opts.get("host", "0.0.0.0"s))),
	opts.get<uint16_t>("port", 8448L)
}
,a
{
	ios::get()
}
{
	static const auto &max_connections
	{
		//TODO: XXX
		//boost::asio::ip::tcp::socket::max_connections   <-- linkage failed?
		std::min(opts.get<uint>("max_connections", SOMAXCONN), uint(SOMAXCONN))
	};

	static const ip::tcp::acceptor::reuse_address reuse_address
	{
		true
	};

	configure(opts);

	log::debug
	{
		log, "%s configured listener SSL", string(logheadbuf, *this)
	};

	a.open(ep.protocol());
	a.set_option(reuse_address);
	log::debug
	{
		log, "%s opened listener socket", string(logheadbuf, *this)
	};

	a.bind(ep);
	log::debug
	{
		log, "%s bound listener socket", string(logheadbuf, *this)
	};

	a.listen(backlog);
	log::debug
	{
		log, "%s listening (backlog: %lu, max connections: %zu)",
		string(logheadbuf, *this),
		backlog,
		max_connections
	};
}
catch(const boost::system::system_error &e)
{
	throw_system_error(e);
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
	log::error
	{
		log, "acceptor(%p) join: %s",
		this,
		e.what()
	};
}

bool
ircd::net::listener::acceptor::interrupt()
noexcept try
{
	interrupting = true;
	a.cancel();
	return true;
}
catch(const boost::system::system_error &e)
{
	log::error
	{
		log, "acceptor(%p) interrupt: %s",
		this,
		string(e)
	};

	return false;
}

/// Sets the next asynchronous handler to start the next accept sequence.
/// Each call to next() sets one handler which handles the connect for one
/// socket. After the connect, an asynchronous SSL handshake handler is set
/// for the socket.
bool
ircd::net::listener::acceptor::set_handle()
try
{
	assert(!handle_set);
	handle_set = true;
	auto sock
	{
		std::make_shared<ircd::socket>(ssl)
	};

	++accepting;
	ip::tcp::socket &sd(*sock);
	a.async_accept(sd, std::bind(&acceptor::accept, this, ph::_1, sock, weak_from(*this)));
	return true;
}
catch(const std::exception &e)
{
	throw panic
	{
		"%s: %s", string(logheadbuf, *this), e.what()
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

	assert(bool(sock));
	assert(handle_set);
	assert(accepting > 0);

	handle_set = false;
	--accepting;
	log::debug
	{
		log, "%s: accepted(%zu) %s %s",
		string(logheadbuf, *this),
		accepting,
		loghead(*sock),
		string(ec)
	};

	if(!check_accept_error(ec, *sock))
		return;

	// Call the proffer-callback if available. This allows the application
	// to check whether to allow or deny this remote before the handshake.
	if(pcb && !pcb(*listener, remote_ipport(*sock)))
	{
		net::close(*sock, dc::RST, close_ignore);
		return;
	}

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
	assert(bool(sock));
	log::debug
	{
		log, "%s: acceptor interrupted %s %s",
		string(logheadbuf, *this),
		loghead(*sock),
		string(ec)
	};

	error_code ec_;
	sock->sd.close(ec_);
	assert(!ec_);
	joining.notify_all();
}
catch(const std::system_error &e)
{
	assert(bool(sock));
	log::derror
	{
		log, "%s: %s in accept(): %s",
		string(logheadbuf, *this),
		loghead(*sock),
		e.what()
	};

	error_code ec_;
	sock->sd.close(ec_);
	assert(!ec_);
}
catch(const std::exception &e)
{
	assert(bool(sock));
	log::error
	{
		log, "%s: %s in accept(): %s",
		string(logheadbuf, *this),
		loghead(*sock),
		e.what()
	};

	error_code ec_;
	sock->sd.close(ec_);
	assert(!ec_);
}

/// Error handler for the accept socket callback. This handler determines
/// whether or not the handler should return or continue processing the
/// result.
///
bool
ircd::net::listener::acceptor::check_accept_error(const error_code &ec,
                                                  socket &sock)
{
	using std::errc;

	if(unlikely(interrupting))
		throw ctx::interrupted();

	if(likely(!ec))
		return true;

	if(system_category(ec)) switch(ec.value())
	{
		case int(errc::operation_canceled):
			return false;

		default:
			break;
	}

	throw_system_error(ec);
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
	log::debug
	{
		log, "%s handshook(%zu) %s",
		loghead(*sock),
		handshaking,
		string(ec)
	};

	check_handshake_error(ec, *sock);
	sock->cancel_timeout();
	assert(bool(cb));
	cb(*listener, sock);
}
catch(const ctx::interrupted &e)
{
	assert(bool(sock));
	log::debug
	{
		log, "%s: SSL handshake interrupted %s %s",
		string(logheadbuf, *this),
		loghead(*sock),
		string(ec)
	};

	close(*sock, dc::RST, close_ignore);
	joining.notify_all();
}
catch(const std::system_error &e)
{
	assert(bool(sock));
	log::derror
	{
		log, "%s: %s in handshake(): %s",
		string(logheadbuf, *this),
		loghead(*sock),
		e.what()
	};

	close(*sock, dc::RST, close_ignore);
}
catch(const std::exception &e)
{
	assert(bool(sock));
	log::error
	{
		log, "%s: %s in handshake(): %s",
		string(logheadbuf, *this),
		loghead(*sock),
		e.what()
	};

	close(*sock, dc::RST, close_ignore);
}

/// Error handler for the SSL handshake callback. This handler determines
/// whether or not the handler should return or continue processing the
/// result.
///
void
ircd::net::listener::acceptor::check_handshake_error(const error_code &ec,
                                                     socket &sock)
{
	using std::errc;

	if(unlikely(interrupting))
		throw ctx::interrupted();

	if(likely(system_category(ec))) switch(ec.value())
	{
		case 0:
			return;

		case int(errc::operation_canceled):
			if(sock.timedout)
				throw_system_error(errc::timed_out);
			else
				break;

		default:
			break;
	}

	throw_system_error(ec);
}

void
ircd::net::listener::acceptor::configure(const json::object &opts)
{
	log::debug
	{
		log, "%s preparing listener socket configuration...", string(logheadbuf, *this)
	};

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
		log::notice
		{
			log, "%s asking for password with purpose '%s' (size: %zu)",
			string(logheadbuf, *this),
			purpose,
			size
		};

		//XXX: TODO
		assert(0);
		return "foobar";
	});

	if(opts.has("certificate_chain_path"))
	{
		const std::string filename
		{
			unquote(opts["certificate_chain_path"])
		};

		if(!fs::exists(filename))
			throw error
			{
				"%s: SSL certificate chain file @ `%s' not found",
				string(logheadbuf, *this),
				filename
			};

		ssl.use_certificate_chain_file(filename);
		log::info
		{
			log, "%s using certificate chain file '%s'",
			string(logheadbuf, *this),
			filename
		};
	}

	if(opts.has("certificate_pem_path"))
	{
		const std::string filename
		{
			unquote(opts.get("certificate_pem_path", name + ".crt"))
		};

		if(!fs::exists(filename))
			throw error
			{
				"%s: SSL certificate pem file @ `%s' not found",
				string(logheadbuf, *this),
				filename
			};

		ssl.use_certificate_file(filename, asio::ssl::context::pem);
		log::info
		{
			log, "%s using certificate file '%s'",
			string(logheadbuf, *this),
			filename
		};
	}

	if(opts.has("private_key_pem_path"))
	{
		const std::string filename
		{
			unquote(opts.get("private_key_pem_path", name + ".crt.key"))
		};

		if(!fs::exists(filename))
			throw error
			{
				"%s: SSL private key file @ `%s' not found",
				string(logheadbuf, *this),
				filename
			};

		ssl.use_private_key_file(filename, asio::ssl::context::pem);
		log::info
		{
			log, "%s using private key file '%s'",
			string(logheadbuf, *this),
			filename
		};
	}

	if(opts.has("tmp_dh_path") && !empty(unquote(opts.at("tmp_dh_path"))))
	{
		const std::string filename
		{
			unquote(opts.at("tmp_dh_path"))
		};

		if(!fs::exists(filename))
			throw error
			{
				"%s: SSL tmp dh file @ `%s' not found",
				string(logheadbuf, *this),
				filename
			};

		ssl.use_tmp_dh_file(filename);
		log::info
		{
			log, "%s using tmp dh file '%s'",
			string(logheadbuf, *this),
			filename
		};
	}
	else if(opts.has("tmp_dh"))
	{
		const const_buffer buf
		{
			unquote(opts.at("tmp_dh"))
		};

		ssl.use_tmp_dh(buf);
		log::info
		{
			log, "%s using DH params supplied in options (%zu bytes)",
			string(logheadbuf, *this),
			size(buf)
		};
	}
	else
	{
		const const_buffer &buf
		{
			openssl::rfc3526_dh_params_pem
		};

		ssl.use_tmp_dh(buf);
		log::info
		{
			log, "%s using pre-supplied rfc3526 DH parameters.",
			string(logheadbuf, *this)
		};
	}
}

//
// listener_udp::acceptor
//

std::ostream &
ircd::net::operator<<(std::ostream &s, const struct listener_udp::acceptor &a)
{
	s << "'" << a.name << "' @ [" << string(a.ep.address()) << "]:" << a.ep.port();
	return s;
}

//
// listener_udp::acceptor::acceptor
//

ircd::net::listener_udp::acceptor::acceptor(const string_view &name,
                                            const json::object &opts)
try
:name
{
	name
}
,opts
{
	opts
}
,ep
{
	ip::address::from_string(unquote(opts.get("host", "0.0.0.0"s))),
	opts.get<uint16_t>("port", 8448L)
}
,a
{
	ios::get()
}
{
	static const ip::udp::socket::reuse_address reuse_address
	{
		true
	};

	a.open(ep.protocol());
	a.set_option(reuse_address);
	log::debug
	{
		log, "%s opened listener socket", string(logheadbuf, *this)
	};

	a.bind(ep);
	log::debug
	{
		log, "%s bound listener socket", string(logheadbuf, *this)
	};
}
catch(const boost::system::system_error &e)
{
	throw_system_error(e);
}

ircd::net::listener_udp::acceptor::~acceptor()
noexcept
{
}

void
ircd::net::listener_udp::acceptor::join()
noexcept try
{
	interrupt();
	joining.wait([this]
	{
		return waiting == 0;
	});
}
catch(const std::exception &e)
{
	log::error
	{
		log, "acceptor(%p) join: %s", this, e.what()
	};
}

bool
ircd::net::listener_udp::acceptor::interrupt()
noexcept try
{
	a.cancel();
	return true;
}
catch(const boost::system::system_error &e)
{
	log::error
	{
		log, "acceptor(%p) interrupt: %s", this, string(e)
	};

	return false;
}

ircd::net::listener_udp::datagram &
ircd::net::listener_udp::acceptor::operator()(datagram &datagram)
{
	assert(ctx::current);

	const auto flags
	{
		this->flags(datagram.flag)
	};

	const auto interruption{[this]
	(ctx::ctx *const &)
	{
		this->interrupt();
	}};

	const scope_count waiting
	{
		this->waiting
	};

	ip::udp::endpoint ep;
	size_t rlen; continuation
	{
		continuation::asio_predicate, interruption, [this, &rlen, &datagram, &ep, &flags]
		(auto &yield)
		{
			rlen = a.async_receive_from(datagram.mbufs, ep, flags, yield);
		}
	};

	datagram.remote = make_ipport(ep);
	datagram.mbuf = {data(datagram.mbuf), rlen};
	return datagram;
}

boost::asio::ip::udp::socket::message_flags
ircd::net::listener_udp::acceptor::flags(const flag &flag)
{
	ip::udp::socket::message_flags ret{0};

	if(flag & flag::PEEK)
		ret |= ip::udp::socket::message_peek;

	return ret;
}

//
// listener_udp::datagram
//

ircd::net::listener_udp::datagram::datagram(const const_buffer &buf,
                                            const ipport &remote,
                                            const enum flag &flag)
:cbuf{buf}
,cbufs{&cbuf, 1}
,remote{remote}
,flag{flag}
{}

ircd::net::listener_udp::datagram::datagram(const mutable_buffer &buf,
                                            const enum flag &flag)
:mbuf{buf}
,mbufs{&mbuf, 1}
,flag{flag}
{}

///////////////////////////////////////////////////////////////////////////////
//
// net/scope_timeout.h
//

ircd::net::scope_timeout::scope_timeout(socket &socket,
                                        const milliseconds &timeout)
:s
{
	timeout < 0ms? nullptr : &socket
}
{
	if(timeout < 0ms)
		return;

	socket.set_timeout(timeout);
}

ircd::net::scope_timeout::scope_timeout(socket &socket,
                                        const milliseconds &timeout,
                                        handler callback)
:s
{
	timeout < 0ms? nullptr : &socket
}
{
	if(timeout < 0ms)
		return;

	socket.set_timeout(timeout, [callback(std::move(callback))]
	(const error_code &ec)
	{
		const bool &timed_out{!ec}; // success = timeout
		callback(timed_out);
	});
}

ircd::net::scope_timeout::scope_timeout(scope_timeout &&other)
noexcept
:s{std::move(other.s)}
{
	other.s = nullptr;
}

ircd::net::scope_timeout &
ircd::net::scope_timeout::operator=(scope_timeout &&other)
noexcept
{
	this->~scope_timeout();
	s = std::move(other.s);
	return *this;
}

ircd::net::scope_timeout::~scope_timeout()
noexcept
{
	cancel();
}

bool
ircd::net::scope_timeout::cancel()
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
	log::error
	{
		log, "socket(%p) scope_timeout::cancel: %s",
		(const void *)s,
		e.what()
	};

	return false;
}

bool
ircd::net::scope_timeout::release()
{
	const auto s{this->s};
	this->s = nullptr;
	return s != nullptr;
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
                          boost::asio::io_service &ios)
:sd
{
	ios
}
,ssl
{
	this->sd, ssl
}
,timer
{
	ios
}
{
	++instances;
}

/// The dtor asserts that the socket is not open/connected requiring a
/// an SSL close_notify. There's no more room for async callbacks via
/// shared_ptr after this dtor.
ircd::net::socket::~socket()
noexcept try
{
	assert(instances > 0);
	if(unlikely(--instances == 0))
		net::dock.notify_all();

	if(unlikely(RB_DEBUG_LEVEL && opened(*this)))
		throw panic
		{
			"Failed to ensure socket(%p) is disconnected from %s before dtor.",
			this,
			string(remote())
		};
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "socket(%p) close: %s",
		this,
		e.what()
	};

	return;
}

void
ircd::net::socket::connect(const endpoint &ep,
                           const open_opts &opts,
                           eptr_handler callback)
{
	log::debug
	{
		log, "socket:%lu attempting connect remote[%s] to:%ld$ms",
		this->id,
		string(ep),
		opts.connect_timeout.count()
	};

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
	log::debug
	{
		log, "%s handshaking for '%s' to:%ld$ms",
		loghead(*this),
		common_name(opts),
		opts.handshake_timeout.count()
	};

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

	log::debug
	{
		log, "%s disconnect type:%d user: in:%zu out:%zu",
		loghead(*this),
		uint(opts.type),
		in.bytes,
		out.bytes
	};

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
	log::derror
	{
		log, "socket:%lu disconnect type:%d :%s",
		this->id,
		uint(opts.type),
		e.what()
	};

	call_user(callback, make_error_code(e));
}
catch(const std::exception &e)
{
	throw panic
	{
		"socket:%lu disconnect: type: %d: %s",
		this->id,
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
	if(likely(!ec))
		return;

	log::dwarning
	{
		log, "socket:%lu cancel :%s",
		this->id,
		string(ec)
	};
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

		callback(make_system_eptr(ec));
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
	const auto interruption{[this]
	(ctx::ctx *const &)
	{
		this->cancel();
	}};

	const scope_timeout timeout
	{
		*this, opts.timeout
	};

	switch(opts.type)
	{
		case ready::READ: continuation
		{
			continuation::asio_predicate, interruption, [this]
			(auto &yield)
			{
				sd.async_wait(wait_type::wait_read, yield);
			}
		};
		break;

		case ready::WRITE: continuation
		{
			continuation::asio_predicate, interruption, [this]
			(auto &yield)
			{
				sd.async_wait(wait_type::wait_write, yield);
			}
		};
		break;

		case ready::ERROR: continuation
		{
			continuation::asio_predicate, interruption, [this]
			(auto &yield)
			{
				sd.async_wait(wait_type::wait_error, yield);
			}
		};
		break;

		default:
			throw ircd::not_implemented{};
	}
}
catch(const boost::system::system_error &e)
{
	if(e.code() == boost::system::errc::operation_canceled && timedout)
		throw_system_error(std::errc::timed_out);

	throw_system_error(e);
}

/// Asynchronous callback when the socket is ready
///
/// This function calls back the handler when the socket is ready
/// for the operation of the specified type.
///
void
ircd::net::socket::wait(const wait_opts &opts,
                        wait_callback_ec callback)
try
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
catch(const boost::system::system_error &e)
{
	throw_system_error(e);
}

void
ircd::net::socket::handle_ready(const std::weak_ptr<socket> wp,
                                const net::ready type,
                                const ec_handler callback,
                                error_code ec,
                                const size_t bytes)
noexcept try
{
	using std::errc;

	// After life_guard is constructed it is safe to use *this in this frame.
	const life_guard<socket> s{wp};

	if(!timedout && !is(ec, errc::operation_canceled) && !fini)
		cancel_timeout();

	if(timedout && is(ec, errc::operation_canceled))
		ec = make_error_code(errc::timed_out);

	if(unlikely(!ec && !sd.is_open()))
		ec = make_error_code(errc::bad_file_descriptor);

	if(type == ready::READ && !ec && bytes == 0)
		ec = error_code{asio::error::eof, asio::error::get_misc_category()};

	log::debug
	{
		log, "%s ready %s %s avail:%zu:%zu:%d",
		loghead(*this),
		reflect(type),
		string(ec),
		type == ready::READ? bytes : 0UL,
		type == ready::READ? available(*this) : 0UL,
		SSL_pending(ssl.native_handle())
	};

	call_user(callback, ec);
}
catch(const std::bad_weak_ptr &e)
{
	// This handler may still be registered with asio after the socket destructs, so
	// the weak_ptr will indicate that fact. However, this is never intended and is
	// a debug assertion which should be corrected.
	log::warning
	{
		log, "socket(%p) belated callback to handler... (%s)",
		this,
		e.what()
	};

	assert(0);
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "socket(%p) handle: %s",
		this,
		e.what()
	};

	assert(0);
	call_user(callback, ec);
}

void
ircd::net::socket::handle_timeout(const std::weak_ptr<socket> wp,
                                  ec_handler callback,
                                  error_code ec)
noexcept try
{
	if(unlikely(wp.expired()))
		return;

	// We increment our end of the timer semaphore. If the count is still
	// behind the other end of the semaphore, this callback was sitting in
	// the ios queue while the timer was given a new task; any effects here
	// will be erroneously bleeding into the next timeout. However the callback
	// is still invoked to satisfy the user's expectation for receiving it.
	assert(timer_sem[0] < timer_sem[1]);
	if(++timer_sem[0] == timer_sem[1] && timer_set) switch(ec.value())
	{
		// A 'success' for this handler means there was a timeout on the socket
		case 0:
		{
			assert(timedout == false);
			timedout = true;
			sd.cancel();
			break;
		}

		// A cancelation means there was no timeout.
		case int(std::errc::operation_canceled):
		{
			assert(ec.category() == std::system_category());
			assert(timedout == false);
			break;
		}

		// All other errors are unexpected, logged and ignored here.
		default: throw panic
		{
			"socket(%p): unexpected: %s\n", (const void *)this, string(ec)
		};
	}
	else ec = make_error_code(std::errc::operation_canceled);

	if(callback)
		call_user(callback, ec);
}
catch(const boost::system::system_error &e)
{
	using std::errc;

	const auto &ec_(e.code());
	if(system_category(ec_)) switch(ec_.value())
	{
		case int(errc::bad_file_descriptor):
		{
			if(fini)
				break;

			[[fallthrough]];
		}

		default:
		{
			assert(0);
			log::critical
			{
				log, "socket(%p) handle timeout: %s",
				(const void *)this,
				string(e)
			};

			break;
		}
	}

	if(callback)
		call_user(callback, ec_);
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "socket(%p) handle timeout: %s",
		(const void *)this,
		e.what()
	};

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
	using std::errc;

	const life_guard<socket> s{wp};
	log::debug
	{
		log, "%s connect %s",
		loghead(*this),
		string(ec)
	};

	// The timer was set by socket::connect() and may need to be canceled.
	if(!timedout && !is(ec, errc::operation_canceled) && !fini)
		cancel_timeout();

	if(timedout && is(ec, errc::operation_canceled))
		ec = make_error_code(errc::timed_out);

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
	log::warning
	{
		log, "socket(%p) belated callback to handle_connect... (%s)",
		this,
		e.what()
	};

	assert(0);
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "socket(%p) handle_connect: %s",
		this,
		e.what()
	};

	assert(0);
	call_user(callback, ec);
}

void
ircd::net::socket::handle_disconnect(std::shared_ptr<socket> s,
                                     eptr_handler callback,
                                     error_code ec)
noexcept try
{
	using std::errc;

	assert(fini);
	if(!timedout && ec != errc::operation_canceled)
		cancel_timeout();

	if(timedout && ec == errc::operation_canceled)
		ec = make_error_code(errc::timed_out);

	log::debug
	{
		log, "%s disconnect %s",
		loghead(*this),
		string(ec)
	};

	// This ignores EOF and turns it into a success to alleviate user concern.
	if(ec.category() == asio::error::get_misc_category())
		if(ec.value() == asio::error::eof)
			ec = error_code{};

	sd.close();
	call_user(callback, ec);
}
catch(const boost::system::system_error &e)
{
	log::error
	{
		log, "socket(%p) disconnect: %s",
		this,
		e.what()
	};

	assert(0);
	call_user(callback, e.code());
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "socket(%p) disconnect: %s",
		this,
		e.what()
	};

	assert(0);
	call_user(callback, ec);
}

void
ircd::net::socket::handle_handshake(std::weak_ptr<socket> wp,
                                    eptr_handler callback,
                                    error_code ec)
noexcept try
{
	using std::errc;

	const life_guard<socket> s{wp};

	if(!timedout && ec != errc::operation_canceled && !fini)
		cancel_timeout();

	if(timedout && ec == errc::operation_canceled)
		ec = make_error_code(errc::timed_out);

	log::debug
	{
		log, "%s handshake %s",
		loghead(*this),
		string(ec)
	};

	// This is the end of the asynchronous call chain; the user is called
	// back with or without error here.
	call_user(callback, ec);
}
catch(const boost::system::system_error &e)
{
	log::error
	{
		log, "socket(%p) after handshake: %s",
		this,
		e.what()
	};

	assert(0);
	call_user(callback, e.code());
}
catch(const std::bad_weak_ptr &e)
{
	log::warning
	{
		log, "socket(%p) belated callback to handle_handshake... (%s)",
		this,
		e.what()
	};

	assert(0);
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "socket(%p) handle_handshake: %s",
		this,
		e.what()
	};

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
		log::warning
		{
			log, "verify[%s]: %s :%s",
			common_name(opts),
			openssl::get_error_string(stctx),
			openssl::print_subject(buf, cert)
		};
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

		case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
		case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
		case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
		case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
			if(opts.allow_self_signed || opts.allow_self_chain)
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
			thread_local char buf[rfc1035::NAME_BUF_SIZE];
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
		log::debug
		{
			log, "verify[%s]: %s",
			common_name(opts),
			openssl::print_subject(buf, cert)
		};
	}

	return true;
}
catch(const inauthentic &e)
{
	log::error
	{
		log, "Certificate rejected: %s", e.what()
	};

	return false;
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Certificate error: %s", e.what()
	};

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
	log::critical
	{
		log, "socket(%p) async handler: unhandled exception: %s",
		this,
		e.what()
	};

	close(*this, dc::RST, close_ignore);
}

void
ircd::net::socket::call_user(const eptr_handler &callback,
                             const error_code &ec)
noexcept try
{
	if(likely(!ec))
		return callback(std::exception_ptr{});

	callback(make_system_eptr(ec));
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "socket(%p) async handler: unhandled exception: %s",
		this,
		e.what()
	};
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

	timer_set = false;
	timedout = false;
	boost::system::error_code ec;
	timer.cancel(ec);
	assert(!ec);
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

	// The sending-side of the semaphore is incremented here to invalidate any
	// pending/queued callbacks to handle_timeout as to not conflict now. The
	// required companion boolean timer_set is also lit here.
	assert(timer_sem[0] <= timer_sem[1]);
	assert(timer_set == false);
	assert(timedout == false);
	++timer_sem[1];
	timer_set = true;
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

///////////////////////////////////////////////////////////////////////////////
//
// net/dns.h
//

/// Linkage for default opts
decltype(ircd::net::dns::opts_default)
ircd::net::dns::opts_default;

decltype(ircd::net::dns::prefetch_ipport)
ircd::net::dns::prefetch_ipport{[]
(std::exception_ptr, const auto &hostport, const auto &record)
{
	// Do nothing; cache already updated if necessary
}};

decltype(ircd::net::dns::prefetch_SRV)
ircd::net::dns::prefetch_SRV{[]
(std::exception_ptr, const auto &hostport, const auto &record)
{
	// Do nothing; cache already updated if necessary
}};

decltype(ircd::net::dns::prefetch_A)
ircd::net::dns::prefetch_A{[]
(std::exception_ptr, const auto &hostport, const auto &record)
{
	// Do nothing; cache already updated if necessary
}};

/// Convenience composition with a single ipport callback. This is the result of
/// an automatic chain of queries such as SRV and A/AAAA based on the input and
/// intermediate results.
void
ircd::net::dns::resolve(const hostport &hp,
                        const opts &op,
                        callback_ipport_one cb)
{
	using prototype = void (const hostport &, opts, callback_ipport_one);

	static mods::import<prototype> function
	{
		"s_dns", "_resolve_ipport"
	};

	function(hp, op, std::move(cb));
}

/// Convenience callback with a single SRV record which was selected from
/// the vector with stochastic respect for weighting and priority.
void
ircd::net::dns::resolve(const hostport &hp,
                        const opts &op,
                        callback_SRV_one cb)
{
	using prototype = void (const hostport &, opts, callback_SRV_one);

	static mods::import<prototype> function
	{
		"s_dns", "_resolve__SRV"
	};

	function(hp, op, std::move(cb));
}

/// Convenience callback with a single A record which was selected from
/// the vector randomly.
void
ircd::net::dns::resolve(const hostport &hp,
                        const opts &op,
                        callback_A_one cb)
{
	using prototype = void (const hostport &, opts, callback_A_one);

	static mods::import<prototype> function
	{
		"s_dns", "_resolve__A"
	};

	function(hp, op, std::move(cb));
}

/// Fundamental callback with a vector of abstract resource records.
void
ircd::net::dns::resolve(const hostport &hp,
                        const opts &op,
                        callback cb)
{
	using prototype = void (const hostport &, const opts &, callback);

	static mods::import<prototype> function
	{
		"s_dns", "_resolve__"
	};

	function(hp, op, std::move(cb));
}

/// Really assumptional and hacky right now. We're just assuming the SRV
/// key is the first two elements of a dot-delimited string which start
/// with underscores. If that isn't good enough in the future this will rot
/// and become a regression hazard.
ircd::string_view
ircd::net::dns::unmake_SRV_key(const string_view &key)
{
	if(token_count(key, '.') < 3)
		return key;

	if(!startswith(token(key, '.', 0), '_'))
		return key;

	if(!startswith(token(key, '.', 1), '_'))
		return key;

	return tokens_after(key, '.', 1);
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

//
// cache
//

ircd::rfc1035::record *
ircd::net::dns::cache::put_error(const rfc1035::question &question,
                                 const uint &code)
try
{
	using prototype = rfc1035::record *(const rfc1035::question &, const uint &);

	static mods::import<prototype> function
	{
		"s_dns", "_put_error"
	};

	return function(question, code);
}
catch(const mods::unavailable &e)
{
	log::dwarning
	{
		log, "Failed to put error for '%s' in DNS cache :%s",
		question.name,
		e.what()
	};

	return nullptr;
}

ircd::rfc1035::record *
ircd::net::dns::cache::put(const rfc1035::question &question,
                           const rfc1035::answer &answer)
try
{
	using prototype = rfc1035::record *(const rfc1035::question &, const rfc1035::answer &);

	static mods::import<prototype> function
	{
		"s_dns", "_put"
	};

	return function(question, answer);
}
catch(const mods::unavailable &e)
{
	log::dwarning
	{
		log, "Failed to put '%s' in DNS cache :%s",
		question.name,
		e.what()
	};

	return nullptr;
}

/// This function has an opportunity to respond from the DNS cache. If it
/// returns true, that indicates it responded by calling back the user and
/// nothing further should be done for them. If it returns false, that
/// indicates it did not respond and to proceed normally. The response can
/// be of a cached successful result, or a cached error. Both will return
/// true.
bool
ircd::net::dns::cache::get(const hostport &hp,
                           const opts &o,
                           const callback &cb)
try
{
	using prototype = bool (const hostport &, const opts &, const callback &);

	static mods::import<prototype> function
	{
		"s_dns", "_get"
	};

	return function(hp, o, cb);
}
catch(const mods::unavailable &e)
{
	thread_local char buf[128];
	log::dwarning
	{
		log, "Failed to get '%s' from DNS cache :%s",
		string(buf, hp),
		e.what()
	};

	return false;
}

bool
ircd::net::dns::cache::for_each(const string_view &type,
                                const closure &closure)
{
	return for_each(rfc1035::qtype.at(type), closure);
}

bool
ircd::net::dns::cache::for_each(const uint16_t &type,
                                const closure &c)
{
	using prototype = bool (const uint16_t &, const closure &);

	static mods::import<prototype> function
	{
		"s_dns", "_for_each"
	};

	return function(type, c);
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
	s << net::string(buf, t);
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
		is_v4(ipp)? fmt::sprintf
		{
			buf, "%s:%u",
			ip::address_v4{host4(ipp)}.to_string(),
			port(ipp)
		}:
		is_v6(ipp)? fmt::sprintf
		{
			buf, "%s:%u",
			ip::address_v6{std::get<ipp.IP>(ipp).byte}.to_string(),
			port(ipp)
		}:
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
			asio::ip::address_v6 { std::get<ipport.IP>(ipport).byte }, port(ipport)
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
			asio::ip::address_v6 { std::get<ipport.IP>(ipport).byte }, port(ipport)
		}
		: ip::tcp::endpoint
		{
			asio::ip::address_v4 { host4(ipport) }, port(ipport)
		},
	};
}

bool
ircd::net::ipport::cmp_ip::operator()(const ipport &a, const ipport &b)
const
{
	if(is_v4(a) && is_v6(b))
		return true;

	if(is_v6(a) && is_v4(b))
		return false;

	assert((is_v4(a) && is_v4(b)) || (is_v6(a) && is_v6(b)));
	return std::get<a.IP>(a).byte < std::get<b.IP>(b).byte;
}

bool
ircd::net::ipport::cmp_port::operator()(const ipport &a, const ipport &b)
const
{
	return std::get<a.PORT>(a) < std::get<b.PORT>(b);
}

//
// ipport::ipport
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
		std::get<IP>(*this).byte = address.to_v6().to_bytes();
		std::reverse(std::get<IP>(*this).byte.begin(), std::get<IP>(*this).byte.end());
	}
	else host4(*this) = address.to_v4().to_ulong();
}

ircd::net::ipport::ipport(const uint32_t &ip,
                          const uint16_t &p)
{
	std::get<TYPE>(*this) = false;
	host6(*this) = 0;
	host4(*this) = ip;
	port(*this) = p;
}

ircd::net::ipport::ipport(const uint128_t &ip,
                          const uint16_t &p)
{
	std::get<TYPE>(*this) = true;
	host6(*this) = ip;
	port(*this) = p;
}

///////////////////////////////////////////////////////////////////////////////
//
// net/hostport.h
//

decltype(ircd::net::canon_port)
ircd::net::canon_port
{
	8448
};

decltype(ircd::net::canon_service)
ircd::net::canon_service
{
	"matrix"
};

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
		size(host(hp)) + 1 + 5 + 1 // optimistic ':' + portnum
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
