// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::net
{
	ctx::dock dock;
	std::optional<dns::init> _dns_;

	static void init_ipv6();
	static void wait_close_sockets();
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

void
ircd::net::init_ipv6()
{
	if(!enable_ipv6)
	{
		log::warning
		{
			log, "IPv6 is disabled by the configuration."
			" Not checking for usable interfaces."
		};
		return;
	}

	if(!addrs::has_usable_ipv6_interface())
	{
		log::dwarning
		{
			log, "No usable IPv6 interfaces detected."
		};

		enable_ipv6.set("false");
		return;
	}

	log::info
	{
		log, "Detected usable IPv6 interfaces."
		" Server will query AAAA records and attempt IPv6 connections. If this"
		" is an error please set ircd.net.enable_ipv6 to false or start with -no6."
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// init
//

/// Network subsystem initialization
ircd::net::init::init()
{
	init_ipv6();
	sslv23_client.set_verify_mode(asio::ssl::verify_peer);
	sslv23_client.set_default_verify_paths();
	_dns_.emplace();
}

/// Network subsystem shutdown
ircd::net::init::~init()
noexcept
{
	_dns_.reset();
	wait_close_sockets();
}

///////////////////////////////////////////////////////////////////////////////
//
// net/net.h
//

decltype(ircd::net::eof)
ircd::net::eof
{
	make_error_code(boost::system::error_code
	{
		boost::asio::error::eof,
		boost::asio::error::get_misc_category()
	})
};

decltype(ircd::net::enable_ipv6)
ircd::net::enable_ipv6
{
	{ "name",     "ircd.net.enable_ipv6"  },
	{ "default",  true                    },
	{ "persist",  false                   },
};

/// Network subsystem log facility
decltype(ircd::net::log)
ircd::net::log
{
	"net", 'N'
};

ircd::string_view
ircd::net::peer_cert_der_sha256_b64(const mutable_buffer &buf,
                                    const socket &socket)
{
	char shabuf alignas(32) [sha256::digest_size];
	const auto hash
	{
		peer_cert_der_sha256(shabuf, socket)
	};

	return b64::encode_unpadded(buf, hash);
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
	char buf[2][128];
	return fmt::sprintf
	{
		out, "socket:%lu local:%s remote:%s",
		id(socket),
		string(buf[0], local_ipport(socket)),
		string(buf[1], remote_ipport(socket)),
	};
}

ircd::net::ipport
ircd::net::remote_ipport(const socket &socket)
noexcept try
{
	if(!opened(socket))
		return {};

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
	if(!opened(socket))
		return {};

	const auto &ep(socket.local());
	return make_ipport(ep);
}
catch(...)
{
	return {};
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

/// Bytes remaining for transmission (in the kernel)
size_t
ircd::net::writable(const socket &socket)
{
	const ssize_t write_bufsz
	(
		net::write_bufsz(socket)
	);

	const ssize_t flushing
	(
		net::flushing(socket)
	);

	assert(write_bufsz >= flushing);
	return std::max(write_bufsz - flushing, 0L);
}

/// Bytes buffered for transmission (in the kernel)
size_t
ircd::net::flushing(const socket &socket)
{
	const ip::tcp::socket &sd(socket);
	const auto &fd
	{
		mutable_cast(sd).lowest_layer().native_handle()
	};

	long value(0);
	#ifdef TIOCOUTQ
		syscall(::ioctl, fd, TIOCOUTQ, &value);
	#else
		#warning "TIOCOUTQ is not defined on this platform."
	#endif

	return value;
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
	static char buffer[512];

	size_t remain{len}; while(remain)
	{
		const mutable_buffer mb
		{
			buffer, std::min(remain, sizeof(buffer))
		};

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
	static char buffer[512];

	size_t remain{len}; while(remain)
	{
		const mutable_buffer mb
		{
			buffer, std::min(remain, sizeof(buffer))
		};

		size_t read;
		if(!(read = read_one(socket, mb)))
			break;

		remain -= read;
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

/// Bytes available for reading (userspace)
size_t
ircd::net::available(const socket &socket)
noexcept
{
	const ip::tcp::socket &sd(socket);
	boost::system::error_code ec;
	return sd.available(ec);
}

/// Bytes available for reading (kernel)
size_t
ircd::net::readable(const socket &socket)
{
	ip::tcp::socket &sd(const_cast<net::socket &>(socket));
	ip::tcp::socket::bytes_readable command{true};
	sd.io_control(command);
	return command.get();
}

///////////////////////////////////////////////////////////////////////////////
//
// net/check.h
//

void
ircd::net::check(socket &socket,
                 const ready &type)
{
	const error_code ec
	{
		check(std::nothrow, socket, type)
	};

	if(likely(!ec))
		return;

	throw_system_error(ec);
	__builtin_unreachable();
}

std::error_code
ircd::net::check(std::nothrow_t,
                 socket &socket,
                 const ready &type)
noexcept
{
	return socket.check(std::nothrow, type);
}

///////////////////////////////////////////////////////////////////////////////
//
// net/wait.h
//

decltype(ircd::net::wait_opts_default)
ircd::net::wait_opts_default;

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
			close(*s, dc::RST, close_ignore);

		handler(std::move(eptr));
	}};

	const dns::callback_ipport connector{[&socket, opts, complete(std::move(complete))]
	(std::exception_ptr eptr, const hostport &hp, const ipport &ipport)
	{
		if(eptr)
			return complete(std::move(eptr));

		const auto ep{make_endpoint(ipport)};
		socket.connect(ep, opts, std::move(complete));
	}};

	if(!opts.ipport)
		dns::resolve(opts.hostport, dns::opts_default, std::move(connector));
	else
		connector({}, opts.hostport, opts.ipport);
}

///////////////////////////////////////////////////////////////////////////////
//
// net/sopts.h
//

/// Construct sock_opts with the current options from socket argument
ircd::net::sock_opts::sock_opts(const socket &socket)
:v6only{net::v6only(socket)}
,blocking{net::blocking(socket)}
,nodelay{net::nodelay(socket)}
,quickack{net::quickack(socket)}
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
	if(opts.v6only != opts.IGN)
		net::v6only(socket, opts.v6only);

	if(opts.blocking != opts.IGN)
		net::blocking(socket, opts.blocking);

	if(opts.nodelay != opts.IGN)
		net::nodelay(socket, opts.nodelay);

	if(opts.quickack != opts.IGN)
		net::quickack(socket, opts.quickack);

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
	const ip::tcp::socket::send_low_watermark option
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
	const ip::tcp::socket::receive_low_watermark option
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
	const ip::tcp::socket::send_buffer_size option
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
	const ip::tcp::socket::receive_buffer_size option
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
	const ip::tcp::socket::linger option
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
	const ip::tcp::socket::keep_alive option{b};
	ip::tcp::socket &sd(socket);
	sd.set_option(option);
}

void
ircd::net::quickack(socket &socket,
                    const bool &b)
#if defined(TCP_QUICKACK) && defined(SOL_SOCKET)
{
	ip::tcp::socket &sd(socket);
	const auto &fd
	{
		sd.lowest_layer().native_handle()
	};

	const int val(b);
	const socklen_t len(sizeof(val));
	syscall(::setsockopt, fd, SOL_SOCKET, TCP_QUICKACK, &val, len);
}
#else
{
	#warning "TCP_QUICKACK is not defined on this platform."
}
#endif

void
ircd::net::nodelay(socket &socket,
                   const bool &b)
{
	const ip::tcp::no_delay option{b};
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

void
ircd::net::v6only(socket &socket,
                  const bool &b)
{
	const ip::v6_only option{b};
	ip::tcp::socket &sd(socket);
	sd.set_option(option);
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
ircd::net::quickack(const socket &socket)
#if defined(TCP_QUICKACK) && defined(SOL_SOCKET)
{
	const ip::tcp::socket &sd(socket);
	const auto &fd
	{
		mutable_cast(sd).lowest_layer().native_handle()
	};

	uint32_t ret;
	socklen_t len(sizeof(ret));
	syscall(::getsockopt, fd, SOL_SOCKET, TCP_QUICKACK, &ret, &len);
	assert(len <= sizeof(ret));
	return ret;
}
#else
{
	#warning "TCP_QUICKACK is not defined on this platform."
	return false;
}
#endif

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

bool
ircd::net::v6only(const socket &socket)
{
	const ip::tcp::socket &sd(socket);
	ip::v6_only option;
	sd.get_option(option);
	return option.value();
}

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
	other.s = nullptr;
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
		log, "socket(%p) scope_timeout::cancel :%s",
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

decltype(ircd::net::ssl_curve_list)
ircd::net::ssl_curve_list
{
	{ "name",     "ircd.net.ssl.curve.list" },
	{ "default",  string_view{}             },
};

decltype(ircd::net::ssl_cipher_list)
ircd::net::ssl_cipher_list
{
	{ "name",     "ircd.net.ssl.cipher.list" },
	{ "default",  string_view{}              },
};

decltype(ircd::net::ssl_cipher_blacklist)
ircd::net::ssl_cipher_blacklist
{
	{ "name",     "ircd.net.ssl.cipher.blacklist" },
	{ "default",  string_view{}                   },
};

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

decltype(ircd::net::socket::desc_connect)
ircd::net::socket::desc_connect
{
	"ircd.net.socket.connect"
};

decltype(ircd::net::socket::desc_handshake)
ircd::net::socket::desc_handshake
{
	"ircd.net.socket.handshake"
};

decltype(ircd::net::socket::desc_disconnect)
ircd::net::socket::desc_disconnect
{
	"ircd.net.socket.disconnect"
};

decltype(ircd::net::socket::desc_timeout)
ircd::net::socket::desc_timeout
{
	"ircd.net.socket.timeout"
};

decltype(ircd::net::socket::desc_wait)
ircd::net::socket::desc_wait
{
	{ "ircd.net.socket.wait.ready.ANY"   },
	{ "ircd.net.socket.wait.ready.READ"  },
	{ "ircd.net.socket.wait.ready.WRITE" },
	{ "ircd.net.socket.wait.ready.ERROR" },
};

decltype(ircd::net::socket::total_bytes_in)
ircd::net::socket::total_bytes_in
{
	{ "name", "ircd.net.socket.in.total.bytes"                      },
	{ "desc", "The total number of bytes received by all sockets"   },
};

decltype(ircd::net::socket::total_bytes_out)
ircd::net::socket::total_bytes_out
{
	{ "name", "ircd.net.socket.out.total.bytes"                     },
	{ "desc", "The total number of bytes received by all sockets"   },
};

decltype(ircd::net::socket::total_calls_in)
ircd::net::socket::total_calls_in
{
	{ "name", "ircd.net.socket.in.total.calls"                      },
	{ "desc", "The total number of read operations on all sockets"  },
};

decltype(ircd::net::socket::total_calls_out)
ircd::net::socket::total_calls_out
{
	{ "name", "ircd.net.socket.out.total.calls"                      },
	{ "desc", "The total number of write operations on all sockets"  },
};

//
// socket
//

ircd::net::socket::socket(asio::ssl::context &ssl)
:sd
{
	ios::get()
}
,ssl
{
	this->sd, ssl
}
,timer
{
	ios::get()
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

	if(unlikely(opened(*this)))
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
		log, "socket(%p) close :%s",
		this,
		e.what()
	};

	return;
}
catch(...)
{
	log::critical
	{
		log, "socket(%p) close: unexpected",
		this,
	};

	ircd::terminate();
}

void
ircd::net::socket::connect(const endpoint &ep,
                           const open_opts &opts,
                           eptr_handler callback)
{
	char epbuf[128];
	log::debug
	{
		log, "socket:%lu attempting connect remote[%s] to:%ld$ms",
		this->id,
		string(epbuf, ep),
		opts.connect_timeout.count()
	};

	auto connect_handler
	{
		std::bind(&socket::handle_connect, this, weak_from(*this), opts, std::move(callback), ph::_1)
	};

	set_timeout(opts.connect_timeout);
	sd.async_connect(ep, ios::handle(desc_connect, std::move(connect_handler)));
}

void
ircd::net::socket::handshake(const open_opts &opts,
                             eptr_handler callback)
{
	assert(!fini);
	assert(sd.is_open());

	log::debug
	{
		log, "%s handshaking to '%s' for '%s' to:%ld$ms",
		loghead(*this),
		opts.send_sni?
			server_name(opts):
			"<no sni>"_sv,
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

	assert(!fini);
	set_timeout(opts.handshake_timeout);

	if(opts.send_sni && server_name(opts))
		openssl::server_name(*this, server_name(opts));

	ssl.set_verify_callback(std::move(verify_handler));
	ssl.async_handshake(handshake_type::client, ios::handle(desc_handshake, std::move(handshake_handler)));
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

	assert(!fini);
	log::debug
	{
		log, "%s disconnect type:%d user: in:%zu out:%zu",
		loghead(*this),
		uint(opts.type),
		in.bytes,
		out.bytes
	};

	cancel();
	assert(!fini);
	fini = true;

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
			ssl.async_shutdown(ios::handle(desc_disconnect, std::move(disconnect_handler)));
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
		"socket:%lu disconnect: type: %d :%s",
		this->id,
		uint(opts.type),
		e.what()
	};
}

bool
ircd::net::socket::cancel()
noexcept
{
	cancel_timeout();

	boost::system::error_code ec;
	sd.cancel(ec);
	if(unlikely(ec))
	{
		char ecbuf[64];
		log::dwarning
		{
			log, "socket:%lu cancel :%s",
			this->id,
			string(ecbuf, ec)
		};
	}

	return !ec;
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
	assert(!fini);
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
	assert(!fini);
	set_timeout(opts.timeout);
	const unwind_exceptional unset{[this]
	{
		cancel_timeout();
	}};

	auto handle
	{
		std::bind(&socket::handle_ready, this, weak_from(*this), opts.type, std::move(callback), ph::_1)
	};

	switch(opts.type)
	{
		case ready::READ:
		{
			// The problem here is that waiting on the sd doesn't account for bytes
			// read into SSL that we didn't consume yet. If something is stuck in
			// those userspace buffers, the socket won't know about it and perform
			// the wait. ASIO should fix this by adding a ssl::stream.wait() method
			// which will bail out immediately in this case before passing up to the
			// real socket wait.
			static char buf[64];
			static const ilist<mutable_buffer> bufs{buf};
			if(SSL_peek(ssl.native_handle(), buf, sizeof(buf)) > 0)
			{
				ircd::dispatch{desc_wait[1], ios::defer, [handle(std::move(handle))]
				{
					handle(error_code{});
				}};

				return;
			}

			// The problem here is that the wait operation gives ec=success on both a
			// socket error and when data is actually available. We then have to check
			// using a non-blocking peek in the handler. By doing it this way here we
			// just get the error in the handler's ec.
			//sd.async_wait(bufs, sd.message_peek, ios::handle(desc_wait[1], [handle(std::move(handle))]
			sd.async_receive(bufs, sd.message_peek, ios::handle(desc_wait[1], [handle(std::move(handle))]
			(const auto &ec, const size_t bytes)
			{
				handle
				(
					!ec && bytes?
						error_code{}:
					!ec && !bytes?
						net::eof:
						make_error_code(ec)
				);
			}));

			return;
		}

		case ready::WRITE:
		{
			sd.async_wait(wait_type::wait_write, ios::handle(desc_wait[2], std::move(handle)));
			return;
		}

		case ready::ERROR:
		{
			sd.async_wait(wait_type::wait_error, ios::handle(desc_wait[3], std::move(handle)));
			return;
		}

		default: throw ircd::not_implemented{};
	}
}
catch(const boost::system::system_error &e)
{
	throw_system_error(e);
}

std::error_code
ircd::net::socket::check(std::nothrow_t,
                         const ready &type)
noexcept
{
	static char buf[64];
	static const ilist<mutable_buffer> bufs
	{
		buf
	};

	if(!sd.is_open())
		return make_error_code(std::errc::bad_file_descriptor);

	if(fini)
		return make_error_code(std::errc::not_connected);

	std::error_code ret;
	if(SSL_peek(ssl.native_handle(), buf, sizeof(buf)) > 0)
		return ret;

	assert(!blocking(*this));
	boost::system::error_code ec;
	if(sd.receive(bufs, sd.message_peek, ec) > 0)
	{
		assert(!ec.value());
		return ret;
	}

	if(ec.value())
		ret = make_error_code(ec);
	else
		ret = eof;

	if(ret == std::errc::resource_unavailable_try_again)
		ret = {};

	return ret;
}

/// Yields ircd::ctx until buffers are full.
template<class iov>
size_t
ircd::net::socket::read_all(iov&& bufs)
try
{
	static const auto completion
	{
		asio::transfer_all()
	};

	assert(!fini);
	const auto interruption{[this]
	(ctx::ctx *const &)
	{
		this->cancel();
	}};

	size_t ret; continuation
	{
		continuation::asio_predicate, interruption, [this, &ret, &bufs]
		(auto &yield)
		{
			ret = asio::async_read(ssl, std::forward<iov>(bufs), completion, yield);
		}
	};

	if(!ret)
		throw std::system_error
		{
			eof
		};

	++in.calls;
	in.bytes += ret;
	++total_calls_in;
	total_bytes_in += ret;
	return ret;
}
catch(const boost::system::system_error &e)
{
	throw_system_error(e);
}

/// Yields ircd::ctx until remote has sent at least some data.
template<class iov>
size_t
ircd::net::socket::read_few(iov&& bufs)
try
{
	assert(!fini);
	const auto interruption{[this]
	(ctx::ctx *const &)
	{
		this->cancel();
	}};

	size_t ret; continuation
	{
		continuation::asio_predicate, interruption, [this, &ret, &bufs]
		(auto &yield)
		{
			ret = ssl.async_read_some(std::forward<iov>(bufs), yield);
		}
	};

	if(!ret)
		throw std::system_error
		{
			eof
		};

	++in.calls;
	in.bytes += ret;
	++total_calls_in;
	total_bytes_in += ret;
	return ret;
}
catch(const boost::system::system_error &e)
{
	throw_system_error(e);
}

/// Non-blocking; as much as possible without blocking
template<class iov>
size_t
ircd::net::socket::read_any(iov&& bufs)
{
	static const auto completion
	{
		asio::transfer_all()
	};

	assert(!fini);
	assert(!blocking(*this));
	boost::system::error_code ec;
	const size_t ret
	{
		asio::read(ssl, std::forward<iov>(bufs), completion, ec)
	};

	++in.calls;
	in.bytes += ret;
	++total_calls_in;
	total_bytes_in += ret;

	if(likely(!ec))
		return ret;

	if(ec == boost::system::errc::resource_unavailable_try_again)
		return ret;

	throw_system_error(ec);
	__builtin_unreachable();
}

/// Non-blocking; One system call only; never throws eof;
template<class iov>
size_t
ircd::net::socket::read_one(iov&& bufs)
{
	assert(!fini);
	assert(!blocking(*this));
	boost::system::error_code ec;
	const size_t ret
	{
		ssl.read_some(std::forward<iov>(bufs), ec)
	};

	++in.calls;
	in.bytes += ret;
	++total_calls_in;
	total_bytes_in += ret;

	if(likely(!ec))
		return ret;

	if(ec == boost::system::errc::resource_unavailable_try_again)
		return ret;

	throw_system_error(ec);
	__builtin_unreachable();
}

/// Yields ircd::ctx until all buffers are sent.
template<class iov>
size_t
ircd::net::socket::write_all(iov&& bufs)
try
{
	static const auto completion
	{
		asio::transfer_all()
	};

	assert(!fini);
	assert(!blocking(*this));
	const auto interruption{[this]
	(ctx::ctx *const &)
	{
		this->cancel();
	}};

	size_t ret; continuation
	{
		continuation::asio_predicate, interruption, [this, &ret, &bufs]
		(auto &yield)
		{
			ret = asio::async_write(ssl, std::forward<iov>(bufs), completion, yield);
		}
	};

	++out.calls;
	out.bytes += ret;
	++total_calls_out;
	total_bytes_out += ret;
	return ret;
}
catch(const boost::system::system_error &e)
{
	throw_system_error(e);
}

/// Yields ircd::ctx until one or more bytes are sent.
template<class iov>
size_t
ircd::net::socket::write_few(iov&& bufs)
try
{
	assert(!fini);
	assert(!blocking(*this));
	const auto interruption{[this]
	(ctx::ctx *const &)
	{
		this->cancel();
	}};

	size_t ret; continuation
	{
		continuation::asio_predicate, interruption, [this, &ret, &bufs]
		(auto &yield)
		{
			ret = ssl.async_write_some(std::forward<iov>(bufs), yield);
		}
	};

	++out.calls;
	out.bytes += ret;
	++total_calls_out;
	total_bytes_out += ret;
	return ret;
}
catch(const boost::system::system_error &e)
{
	throw_system_error(e);
}

/// Non-blocking; writes as much as possible without blocking
template<class iov>
size_t
ircd::net::socket::write_any(iov&& bufs)
try
{
	static const auto completion
	{
		asio::transfer_all()
	};

	assert(!fini);
	assert(!blocking(*this));
	const size_t ret
	{
		asio::write(ssl, std::forward<iov>(bufs), completion)
	};

	++out.calls;
	out.bytes += ret;
	++total_calls_out;
	total_bytes_out += ret;
	return ret;
}
catch(const boost::system::system_error &e)
{
	throw_system_error(e);
}

/// Non-blocking; Writes one "unit" of data or less; never more.
template<class iov>
size_t
ircd::net::socket::write_one(iov&& bufs)
try
{
	assert(!fini);
	assert(!blocking(*this));
	const size_t ret
	{
		ssl.write_some(std::forward<iov>(bufs))
	};

	++out.calls;
	out.bytes += ret;
	++total_calls_out;
	total_bytes_out += ret;
	return ret;
}
catch(const boost::system::system_error &e)
{
	throw_system_error(e);
}

void
ircd::net::socket::handle_ready(const std::weak_ptr<socket> wp,
                                const net::ready type,
                                const ec_handler callback,
                                error_code ec)
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

	if(unlikely(!ec && fini))
		ec = make_error_code(errc::not_connected);

	#ifdef IRCD_DEBUG_NET_SOCKET_READY
	const auto has_pending
	{
		#if OPENSSL_VERSION_NUMBER >= 0x10100000L
			SSL_has_pending(ssl.native_handle())
		#else
			0
		#endif
	};

	char ecbuf[64];
	log::debug
	{
		log, "%s ready %s %s avail:%zu:%zu:%d:%d",
		loghead(*this),
		reflect(type),
		string(ecbuf, ec),
		type == ready::READ? bytes : 0UL,
		type == ready::READ? available(*this) : 0UL,
		has_pending,
		SSL_pending(ssl.native_handle()),
	};
	#endif

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
		log, "socket(%p) handle :%s",
		this,
		e.what()
	};

	const ctx::exception_handler eh;
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
			"socket(%p): unexpected :%s",
			(const void *)this,
			string(ec)
		};
	}
	else ec = make_error_code(std::errc::operation_canceled);

	if(callback)
		call_user(callback, ec);
}
catch(const boost::system::system_error &e)
{
	using std::errc;

	const auto ec_(e.code());
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
				log, "socket(%p) handle timeout :%s",
				(const void *)this,
				string(e)
			};

			break;
		}
	}

	if(callback)
	{
		const ctx::exception_handler eh;
		call_user(callback, ec_);
	}
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "socket(%p) handle timeout :%s",
		(const void *)this,
		e.what()
	};

	if(callback)
	{
		const ctx::exception_handler eh;
		call_user(callback, ec);
	}
}

void
ircd::net::socket::handle_connect(std::weak_ptr<socket> wp,
                                  const open_opts &opts,
                                  eptr_handler callback,
                                  error_code ec)
noexcept try
{
	using std::errc;

	const life_guard<socket> s{wp};
	char ecbuf[64], epbuf[128];
	log::debug
	{
		log, "%s connect to %s :%s",
		loghead(*this),
		string(epbuf, opts.ipport),
		string(ecbuf, ec)
	};

	// The timer was set by socket::connect() and may need to be canceled.
	if(!timedout && !is(ec, errc::operation_canceled) && !fini)
		cancel_timeout();

	if(timedout && is(ec, errc::operation_canceled))
		ec = make_error_code(errc::timed_out);

	if(!ec && opts.handshake && fini)
		ec = make_error_code(errc::operation_canceled);

	// A connect error; abort here by calling the user back with error.
	if(ec)
		return call_user(callback, ec);

	// Try to set the user's socket options now; if something fails we can
	// invoke their callback with the error from the exception handler.
	if(opts.sopts && !fini)
		set(*this, *opts.sopts);

	// The user can opt out of performing the handshake here.
	if(!opts.handshake)
		return call_user(callback, ec);

	assert(!fini);
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
		log, "socket(%p) handle_connect :%s",
		this,
		e.what()
	};

	const ctx::exception_handler eh;
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

	char ecbuf[64];
	log::debug
	{
		log, "%s disconnect %s",
		loghead(*this),
		string(ecbuf, ec)
	};

	// This ignores EOF and turns it into a success to alleviate user concern.
	if(ec == eof)
		ec = error_code{};

	sd.close();
	call_user(callback, ec);
}
catch(const boost::system::system_error &e)
{
	log::error
	{
		log, "socket(%p) disconnect :%s",
		this,
		e.what()
	};

	const auto code(e.code());
	const ctx::exception_handler eh;
	call_user(callback, code);
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "socket(%p) disconnect :%s",
		this,
		e.what()
	};

	const ctx::exception_handler eh;
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

	#ifdef RB_DEBUG
	const auto *const current_cipher
	{
		!ec?
			openssl::current_cipher(*this):
			nullptr
	};

	char ecbuf[64];
	log::debug
	{
		log, "%s handshake cipher:%s %s",
		loghead(*this),
		current_cipher?
			openssl::name(*current_cipher):
			"<NO CIPHER>"_sv,
		string(ecbuf, ec)
	};
	#endif

	// Toggles the behavior of non-async functions; see func comment
	if(!ec)
		blocking(*this, false);

	// This is the end of the asynchronous call chain; the user is called
	// back with or without error here.
	call_user(callback, ec);
}
catch(const boost::system::system_error &e)
{
	log::error
	{
		log, "socket(%p) after handshake :%s",
		this,
		e.what()
	};

	const auto code(e.code());
	const ctx::exception_handler eh;
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
		log, "socket(%p) handle_handshake :%s",
		this,
		e.what()
	};

	const ctx::exception_handler eh;
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
	// true to still continue.

	// Socket ordered to shut down. We abort the verification here
	// to allow the open_opts out of scope with the user.
	if(fini || !sd.is_open())
		return false;

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
		thread_local char buf[16_KiB];
		const critical_assertion ca;
		log::warning
		{
			log, "verify[%s] :%s :%s",
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
			__builtin_unreachable();

		case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
			assert(openssl::get_error_depth(stctx) == 0);
			if(opts.allow_self_signed)
				return true;

			reject();
			__builtin_unreachable();

		case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
		case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
		case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
		case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
			if(opts.allow_self_signed || opts.allow_self_chain)
				return true;

			reject();
			__builtin_unreachable();

		case X509_V_ERR_CERT_HAS_EXPIRED:
			if(opts.allow_expired)
				return true;

			reject();
			__builtin_unreachable();
	}

	const bool verify_common_name
	{
		opts.verify_common_name &&
		(opts.verify_self_signed_common_name && err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT)
	};

	if(verify_common_name)
	{
		if(unlikely(!common_name(opts)))
			throw inauthentic
			{
				"No common name specified in connection options"
			};

		boost::asio::ssl::rfc2818_verification verifier
		{
			common_name(opts)
		};

		if(!verifier(true, vc))
		{
			thread_local char buf[rfc1035::NAME_BUFSIZE];
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

	#ifdef RB_DEBUG
	thread_local char buf[16_KiB];
	const critical_assertion ca;
	log::debug
	{
		log, "verify[%s] %s",
		common_name(opts),
		openssl::print_subject(buf, cert)
	};
	#endif

	return true;
}
catch(const inauthentic &e)
{
	log::error
	{
		log, "Certificate rejected :%s", e.what()
	};

	return false;
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Certificate error :%s", e.what()
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
		log, "socket(%p) async handler: unhandled exception :%s",
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
		log, "socket(%p) async handler: unhandled exception :%s",
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

	const milliseconds ret
	{
		exp.total_milliseconds()
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
	const boost::posix_time::milliseconds pt
	{
		t.count()
	};

	timer.expires_from_now(pt);
	timer.async_wait(ios::handle(desc_timeout, std::move(handler)));
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
// net/ipport.h
//

std::ostream &
ircd::net::operator<<(std::ostream &s, const ipport &t)
{
	char buf[128];
	s << net::string(buf, t);
	return s;
}

ircd::string_view
ircd::net::string(const mutable_buffer &buf,
                  const ipport &ipp)
{
	mutable_buffer out{buf};
	const bool has_port(port(ipp));
	const bool need_bracket
	{
		has_port && is_v6(ipp) && !is_null(ipp)
	};

	if(need_bracket)
		consume(out, copy(out, '['));

	if(ipp)
		consume(out, size(string(out, std::get<ipport::IP>(ipp))));

	if(need_bracket)
		consume(out, copy(out, ']'));

	if(has_port)
	{
		consume(out, copy(out, ':'));
		consume(out, size(lex_cast(port(ipp), out)));
	}

	return
	{
		data(buf), data(out)
	};
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
		make_address(std::get<ipport::IP>(ipport)), port(ipport)
	};
}

boost::asio::ip::tcp::endpoint
ircd::net::make_endpoint(const ipport &ipport)
{
	return
	{
		make_address(std::get<ipport::IP>(ipport)), port(ipport)
	};
}

//
// cmp
//

bool
ircd::net::ipport::cmp_ip::operator()(const ipport &a, const ipport &b)
const
{
	return ipaddr::cmp()(std::get<ipport::IP>(a), std::get<ipport::IP>(b));
}

bool
ircd::net::ipport::cmp_port::operator()(const ipport &a, const ipport &b)
const
{
	return std::get<ipport::PORT>(a) < std::get<ipport::PORT>(b);
}

///////////////////////////////////////////////////////////////////////////////
//
// net/ipaddr.h
//

boost::asio::ip::address
ircd::net::make_address(const ipaddr &ipaddr)
{
	return is_v4(ipaddr)?
		ip::address(make_address(ipaddr.v4)):
		ip::address(make_address(ipaddr.v6));
}

boost::asio::ip::address
ircd::net::make_address(const string_view &ip)
try
{
	return
		ip && ip == "*"?
			boost::asio::ip::address_v6::any():
		ip?
			boost::asio::ip::make_address(ip):
			boost::asio::ip::address{};
}
catch(const boost::system::system_error &e)
{
	throw_system_error(e);
}

boost::asio::ip::address_v4
ircd::net::make_address(const uint32_t &ip)
{
	return ip::address_v4{ip};
}

boost::asio::ip::address_v6
ircd::net::make_address(const uint128_t &ip)
{
	const auto &pun
	{
		reinterpret_cast<const uint8_t (&)[16]>(ip)
	};

	auto punpun
	{
		reinterpret_cast<const std::array<uint8_t, 16> &>(pun)
	};

	std::reverse(begin(punpun), end(punpun));
	return ip::address_v6{punpun};
}

std::ostream &
ircd::net::operator<<(std::ostream &s, const ipaddr &ipa)
{
	char buf[128];
	s << net::string(buf, ipa);
	return s;
}

ircd::string_view
ircd::net::string(const mutable_buffer &buf,
                  const ipaddr &ipaddr)
{
	return is_v4(ipaddr)?
		string_ip4(buf, ipaddr.v4):
		string_ip6(buf, ipaddr.v6);
}

ircd::string_view
ircd::net::string_ip4(const mutable_buffer &buf,
                      const uint32_t &ip)
{
	return string(buf, make_address(ip));
}

ircd::string_view
ircd::net::string_ip6(const mutable_buffer &buf,
                      const uint128_t &ip)
{
	return string(buf, make_address(ip));
}

bool
ircd::net::is_loop(const ipaddr &ipaddr)
{
	return is_v4(ipaddr)?
		make_address(ipaddr.v4).is_loopback():
		make_address(ipaddr.v6).is_loopback();
}

//
// ipaddr::ipaddr
//

static_assert
(
	SIZEOF_LONG_LONG >= 8,
	"8 byte integer literals are required."
);

decltype(ircd::net::ipaddr::v4_max)
ircd::net::ipaddr::v4_min
{
	0x0000ffff00000000ULL
};

decltype(ircd::net::ipaddr::v4_max)
ircd::net::ipaddr::v4_max
{
	v4_min +
	0x00000000ffffffffULL
};

ircd::net::ipaddr::ipaddr(const string_view &ip)
:ipaddr
{
	make_address(ip)
}
{
}

ircd::net::ipaddr::ipaddr(const rfc1035::record::A &rr)
:ipaddr
{
	rr.ip4
}
{
}

ircd::net::ipaddr::ipaddr(const rfc1035::record::AAAA &rr)
:ipaddr
{
	rr.ip6
}
{
}

ircd::net::ipaddr::ipaddr(const uint32_t &ip)
:ipaddr
{
	make_address(ip)
}
{
}

ircd::net::ipaddr::ipaddr(const uint128_t &ip)
:ipaddr
{
	make_address(ip)
}
{
}

ircd::net::ipaddr::ipaddr(const asio::ip::address &address)
{
	const auto address_
	{
		address.is_v6()?
			address.to_v6():
			make_address_v6(ip::v4_mapped, address.to_v4())
	};

	byte = address_.to_bytes();
	std::reverse(byte.begin(), byte.end());
}

//
// ipaddr::ipaddr
//

bool
ircd::net::ipaddr::cmp::operator()(const ipaddr &a, const ipaddr &b)
const
{
	return a.byte < b.byte;
}

///////////////////////////////////////////////////////////////////////////////
//
// net/hostport.h
//

/// Creates a host:service or host:port pair from the single string literally
/// containing the colon deliminated values. If the suffix is a port number
/// then the behavior for the port number constructor applies; if a service
/// string then the service constructor applies.
ircd::net::hostport::hostport(const string_view &amalgam)
:host
{
	rfc3986::host(amalgam)
}
,port
{
	rfc3986::port(amalgam)
}
{
	// When the amalgam has no port || a valid integer port
	if(amalgam == host || port)
		return;

	// When the port is actually a service string
	this->service = rsplit(amalgam, ':').second;
}

ircd::net::hostport::hostport(const string_view &amalgam,
                              verbatim_t)
:host
{
	rfc3986::host(amalgam)
}
,service
{
	amalgam != host && !rfc3986::port(amalgam)?
		rsplit(amalgam, ':').second:
		string_view{}
}
,port
{
	rfc3986::port(amalgam)
}
{}

std::ostream &
ircd::net::operator<<(std::ostream &s, const hostport &t)
{
	thread_local char buf[rfc3986::DOMAIN_BUFSIZE * 2];
	const critical_assertion ca;
	s << string(buf, t);
	return s;
}

std::string
ircd::net::canonize(const hostport &hostport)
{
	const size_t len
	{
		size(host(hostport))            // host
		+ 1 + size(service(hostport))   // ':' + service
		+ 1 + 5 + 1                     // ':' + portnum  (optimistic)
	};

	return ircd::string(len, [&hostport]
	(const mutable_buffer &buf)
	{
		return canonize(buf, hostport);
	});
}

ircd::string_view
ircd::net::canonize(const mutable_buffer &buf,
                    const hostport &hostport)
{
	thread_local char svc[32], tlbuf[2][rfc3986::DOMAIN_BUFSIZE * 2];
	assert(service(hostport) || port(hostport));

	const string_view &service_name
	{
		!service(hostport)?
			net::dns::service_name(std::nothrow, svc, port(hostport), "tcp"):
			service(hostport)
	};

	if(likely(service_name))
		return fmt::sprintf
		{
			buf, "%s:%s",
			tolower(tlbuf[0], host(hostport)),
			tolower(tlbuf[1], service_name),
		};

	if(unlikely(!port(hostport)))
		throw error
		{
			"Missing service suffix in hostname:service string.",
		};

	return fmt::sprintf
	{
		buf, "%s:%u",
		tolower(tlbuf[0], host(hostport)),
		port(hostport),
	};
}

ircd::string_view
ircd::net::string(const mutable_buffer &buf,
                  const hostport &hp)
{
	thread_local char tlbuf[2][rfc3986::DOMAIN_BUFSIZE * 2];

	if(empty(service(hp)) && port(hp) == 0)
		return fmt::sprintf
		{
			buf, "%s",
			tolower(tlbuf[0], host(hp)),
		};

	if(empty(service(hp)) && port(hp) != 0)
		return fmt::sprintf
		{
			buf, "%s:%u",
			tolower(tlbuf[0], host(hp)),
			port(hp)
		};

	if(!empty(service(hp)) && port(hp) == 0)
		return fmt::sprintf
		{
			buf, "%s:%s",
			tolower(tlbuf[0], host(hp)),
			tolower(tlbuf[1], service(hp)),
		};

	return fmt::sprintf
	{
		buf, "%s:%u (%s)",
		tolower(tlbuf[0], host(hp)),
		port(hp),
		tolower(tlbuf[1], service(hp)),
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// net/asio.h
//

std::string
ircd::net::string(const ip::tcp::endpoint &ep)
{
	return string(make_ipport(ep));
}

ircd::string_view
ircd::net::string(const mutable_buffer &buf,
                  const ip::tcp::endpoint &ep)
{
	return string(buf, make_ipport(ep));
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
ircd::net::string(const ip::address &addr)
{
	return
		addr.is_v4()?
			string(addr.to_v4()):
			string(addr.to_v6());
}

std::string
ircd::net::string(const ip::address_v4 &addr)
{
	return util::string(16, [&addr]
	(const mutable_buffer &out)
	{
		return string(out, addr);
	});
}

std::string
ircd::net::string(const ip::address_v6 &addr)
{
	return addr.to_string();
}

ircd::string_view
ircd::net::string(const mutable_buffer &out,
                  const ip::address &addr)
{
	return
		addr.is_v4()?
			string(out, addr.to_v4()):
			string(out, addr.to_v6());
}

ircd::string_view
ircd::net::string(const mutable_buffer &out,
                  const ip::address_v4 &addr)
{
	const uint32_t a(addr.to_ulong());
	return fmt::sprintf
	{
		out, "%u.%u.%u.%u",
		(a & 0xFF000000U) >> 24,
		(a & 0x00FF0000U) >> 16,
		(a & 0x0000FF00U) >> 8,
		(a & 0x000000FFU) >> 0,
	};
}

ircd::string_view
ircd::net::string(const mutable_buffer &out,
                  const ip::address_v6 &addr)
{
	return
	{
		data(out), string(addr).copy(data(out), size(out))
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
