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
	thread_local char buffer[512];

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
	thread_local char buffer[512];

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

std::string
ircd::net::cipher_list(const acceptor &a)
{
	auto &ssl(const_cast<acceptor &>(a).ssl);
	return openssl::cipher_list(*ssl.native_handle());
}

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
		acceptor->close();
}

ircd::string_view
ircd::net::listener::name()
const
{
	const net::acceptor &a(*this);
	return net::name(a);
}

ircd::net::listener::operator
ircd::json::object()
const
{
	const net::acceptor &a(*this);
	return net::config(a);
}

ircd::net::listener::operator
net::acceptor &()
{
	assert(acceptor);
	return *acceptor;
}

ircd::net::listener::operator
const net::acceptor &()
const
{
	assert(acceptor);
	return *acceptor;
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
	std::make_unique<net::acceptor_udp>(name, opts)
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

//
// acceptor
//

decltype(ircd::net::acceptor::log)
ircd::net::acceptor::log
{
	"net.listen"
};

decltype(ircd::net::acceptor::timeout)
ircd::net::acceptor::timeout
{
	{ "name",     "ircd.net.acceptor.timeout" },
	{ "default",  12000L                      },
};

/// The number of simultaneous handshakes we conduct across all clients.
decltype(ircd::net::acceptor::handshaking_max)
ircd::net::acceptor::handshaking_max
{
	{ "name",     "ircd.net.acceptor.handshaking.max" },
	{ "default",  64L                                 },
};

/// The number of simultaneous handshakes we conduct for a single peer (which
/// is an IP without a port in this context). This prevents a peer from
/// reaching the handshaking.max limit to DoS out other peers.
decltype(ircd::net::acceptor::handshaking_max_per_peer)
ircd::net::acceptor::handshaking_max_per_peer
{
	{ "name",     "ircd.net.acceptor.handshaking.max_per_peer" },
	{ "default",  16L                                          },
};

decltype(ircd::net::acceptor::ssl_curve_list)
ircd::net::acceptor::ssl_curve_list
{
	{ "name",     "ircd.net.acceptor.ssl.curve.list"     },
	{ "default",  string_view{ircd::net::ssl_curve_list} },
};

decltype(ircd::net::acceptor::ssl_cipher_list)
ircd::net::acceptor::ssl_cipher_list
{
	{ "name",     "ircd.net.acceptor.ssl.cipher.list"     },
	{ "default",  string_view{ircd::net::ssl_cipher_list} },
};

decltype(ircd::net::acceptor::ssl_cipher_blacklist)
ircd::net::acceptor::ssl_cipher_blacklist
{
	{ "name",     "ircd.net.acceptor.ssl.cipher.blacklist"     },
	{ "default",  string_view{ircd::net::ssl_cipher_blacklist} },
};

bool
ircd::net::stop(acceptor &a)
{
	a.close();
	return true;
}

bool
ircd::net::start(acceptor &a)
{
	if(!a.a.is_open())
		a.open();

	allow(a);
	return true;
}

bool
ircd::net::allow(acceptor &a)
{
	if(unlikely(!a.a.is_open()))
		return false;

	if(a.accepting > 0)
		return false;

	a.set_handle();
	return true;
}

std::ostream &
ircd::net::operator<<(std::ostream &s, const acceptor &a)
{
	s << loghead(a);
	return s;
}

ircd::string_view
ircd::net::loghead(const acceptor &a)
{
	thread_local char buf[512];
	return loghead(buf, a);
}

ircd::string_view
ircd::net::loghead(const mutable_buffer &out,
                   const acceptor &a)
{
	thread_local char addrbuf[128];
	return fmt::sprintf
	{
		out, "[%s] @ [%s]:%u",
		name(a),
		string(addrbuf, a.ep.address()),
		a.ep.port(),
	};
}

ircd::net::ipport
ircd::net::local(const acceptor &a)
{
	return make_ipport(a.a.local_endpoint());
}

ircd::net::ipport
ircd::net::binder(const acceptor &a)
{
	return make_ipport(a.ep);
}

ircd::string_view
ircd::net::name(const acceptor &a)
{
	return a.name;
}

ircd::json::object
ircd::net::config(const acceptor &a)
{
	return a.opts;
}

size_t
ircd::net::accepting_count(const acceptor &a)
{
	return a.accepting;
}

size_t
ircd::net::handshaking_count(const acceptor &a)
{
	return a.handshaking.size();
}

size_t
ircd::net::handshaking_count(const acceptor &a,
                             const ipaddr &ipaddr)
{
	return std::count_if(begin(a.handshaking), end(a.handshaking), [&ipaddr]
	(const auto &socket_p)
	{
		return remote_ipport(*socket_p) == ipaddr;
	});
}

//
// acceptor::acceptor
//

ircd::net::acceptor::acceptor(net::listener &listener,
                              const string_view &name,
                              const json::object &opts,
                              listener::callback cb,
                              listener::proffer pcb)
try
:listener_
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
	make_address(unquote(opts.get("host", "*"_sv))),
	opts.get<uint16_t>("port", 8448L)
}
,a
{
	ios::get()
}
{
	configure(opts);

	log::debug
	{
		log, "%s configured listener SSL",
		loghead(*this)
	};

	open();
}
catch(const boost::system::system_error &e)
{
	throw_system_error(e);
}

ircd::net::acceptor::~acceptor()
noexcept
{
	if(accepting || !handshaking.empty())
		log::critical
		{
			"The acceptor must not have clients during destruction!"
			" (accepting:%zu handshaking:%zu)",
			accepting,
			handshaking.size(),
		};
}

void
ircd::net::acceptor::open()
{
	static const auto &max_connections
	{
		//TODO: XXX
		//boost::asio::ip::tcp::socket::max_connections   <-- linkage failed?
		std::min(json::object(opts).get<uint>("max_connections", SOMAXCONN), uint(SOMAXCONN))
	};

	static const ip::tcp::acceptor::reuse_address reuse_address
	{
		true
	};

	assert(!interrupting);
	interrupting = false;
	a.open(ep.protocol());
	a.set_option(reuse_address);
	a.non_blocking(true);
	log::debug
	{
		log, "%s opened listener socket",
		loghead(*this)
	};

	a.bind(ep);
	log::debug
	{
		log, "%s bound listener socket",
		loghead(*this)
	};

	a.listen(backlog);
	log::debug
	{
		log, "%s listening (backlog: %lu, max connections: %zu)",
		loghead(*this),
		backlog,
		max_connections
	};
}

void
ircd::net::acceptor::close()
{
	if(!interrupting)
		interrupt();

	if(a.is_open())
		a.close();

	for(const auto &sock : handshaking)
		sock->cancel();

	join();
	log::debug
	{
		log, "%s listener finished",
		loghead(*this)
	};
}

void
ircd::net::acceptor::join()
noexcept try
{
	if(!interrupting)
		interrupt();

	if(!ctx::current)
		return;

	joining.wait([this]
	{
		return !accepting && handshaking.empty();
	});

	interrupting = false;
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
ircd::net::acceptor::interrupt()
noexcept try
{
	if(interrupting)
		return false;

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
ircd::net::acceptor::set_handle()
try
{
	static ios::descriptor desc
	{
		"ircd::net::acceptor accept"
	};

	const auto &sock
	{
		std::make_shared<ircd::socket>(ssl)
	};

	auto handler
	{
		std::bind(&acceptor::accept, this, ph::_1, sock)
	};

	ip::tcp::socket &sd(*sock);
	a.async_accept(sd, ios::handle(desc, std::move(handler)));
	++accepting;
	return true;
}
catch(const std::exception &e)
{
	throw panic
	{
		"%s :%s", loghead(*this), e.what()
	};
}

/// Callback for a socket connected. This handler then invokes the
/// asynchronous SSL handshake sequence.
///
void
ircd::net::acceptor::accept(const error_code &ec,
                            const std::shared_ptr<socket> sock)
noexcept try
{
	assert(bool(sock));
	assert(accepting > 0);
	assert(accepting == 1); // for now
	thread_local char ecbuf[64];
	log::debug
	{
		log, "%s %s accepted(%zu) %s",
		loghead(*sock),
		loghead(*this),
		accepting,
		string(ecbuf, ec)
	};

	--accepting;
	if(!check_accept_error(ec, *sock))
		return;

	const auto &remote
	{
		remote_ipport(*sock)
	};

	// Call the proffer-callback if available. This allows the application
	// to check whether to allow or deny this remote before the handshake.
	if(pcb && !pcb(*listener_, remote))
	{
		net::close(*sock, dc::RST, close_ignore);
		return;
	}

	if(unlikely(handshaking_count(*this) >= size_t(handshaking_max)))
	{
		log::dwarning
		{
			log, "%s refusing to handshake %s; exceeds maximum of %zu handshakes.",
			loghead(*sock),
			loghead(*this),
			size_t(handshaking_max),
		};

		net::close(*sock, dc::RST, close_ignore);
		return;
	}

	if(unlikely(handshaking_count(*this, remote) >= size_t(handshaking_max_per_peer)))
	{
		log::dwarning
		{
			log, "%s refusing to handshake %s; exceeds maximum of %zu handshakes to them.",
			loghead(*sock),
			loghead(*this),
			size_t(handshaking_max_per_peer),
		};

		net::close(*sock, dc::RST, close_ignore);
		return;
	}

	static const socket::handshake_type handshake_type
	{
		socket::handshake_type::server
	};

	static ios::descriptor desc
	{
		"ircd::net::acceptor async_handshake"
	};

	const auto it
	{
		handshaking.emplace(end(handshaking), sock)
	};

	auto handshake
	{
		std::bind(&acceptor::handshake, this, ph::_1, sock, it)
	};

	sock->set_timeout(milliseconds(timeout));
	sock->ssl.async_handshake(handshake_type, ios::handle(desc, std::move(handshake)));
}
catch(const ctx::interrupted &e)
{
	assert(bool(sock));

	thread_local char ecbuf[64];
	log::debug
	{
		log, "%s acceptor interrupted %s %s",
		loghead(*sock),
		loghead(*this),
		string(ecbuf, ec)
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
		log, "%s %s in accept(): %s",
		loghead(*sock),
		loghead(*this),
		e.what()
	};

	error_code ec_;
	sock->sd.close(ec_);
	assert(!ec_);
	joining.notify_all();
}
catch(const std::exception &e)
{
	assert(bool(sock));
	log::error
	{
		log, "%s %s in accept(): %s",
		loghead(*sock),
		loghead(*this),
		e.what()
	};

	error_code ec_;
	sock->sd.close(ec_);
	assert(!ec_);
	joining.notify_all();
}

/// Error handler for the accept socket callback. This handler determines
/// whether or not the handler should return or continue processing the
/// result.
///
bool
ircd::net::acceptor::check_accept_error(const error_code &ec,
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
	__builtin_unreachable();
}

void
ircd::net::acceptor::handshake(const error_code &ec,
                               const std::shared_ptr<socket> sock,
                               const decltype(handshaking)::const_iterator it)
noexcept try
{
	assert(bool(sock));
	assert(!handshaking.empty());
	assert(it != end(handshaking));

	#ifdef RB_DEBUG
	const auto *const current_cipher
	{
		!ec?
			openssl::current_cipher(*sock):
			nullptr
	};

	thread_local char ecbuf[64];
	log::debug
	{
		log, "%s %s handshook(%zd:%zu) cipher:%s %s",
		loghead(*sock),
		loghead(*this),
		std::distance(cbegin(handshaking), it),
		handshaking.size(),
		current_cipher?
			openssl::name(*current_cipher):
			"<NO CIPHER>"_sv,
		string(ecbuf, ec)
	};
	#endif

	handshaking.erase(it);
	check_handshake_error(ec, *sock);
	sock->cancel_timeout();
	assert(bool(cb));

	// Toggles the behavior of non-async functions; see func comment
	blocking(*sock, false);
	cb(*listener_, sock);
}
catch(const ctx::interrupted &e)
{
	assert(bool(sock));
	thread_local char ecbuf[64];
	log::debug
	{
		log, "%s SSL handshake interrupted %s %s",
		loghead(*sock),
		loghead(*this),
		string(ecbuf, ec)
	};

	net::close(*sock, dc::RST, close_ignore);
	joining.notify_all();
}
catch(const std::system_error &e)
{
	assert(bool(sock));
	log::derror
	{
		log, "%s %s in handshake(): %s",
		loghead(*sock),
		loghead(*this),
		e.what()
	};

	net::close(*sock, dc::RST, close_ignore);
	joining.notify_all();
}
catch(const std::exception &e)
{
	assert(bool(sock));
	log::error
	{
		log, "%s %s in handshake(): %s",
		loghead(*sock),
		loghead(*this),
		e.what()
	};

	net::close(*sock, dc::RST, close_ignore);
	joining.notify_all();
}

/// Error handler for the SSL handshake callback. This handler determines
/// whether or not the handler should return or continue processing the
/// result.
///
void
ircd::net::acceptor::check_handshake_error(const error_code &ec,
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
	__builtin_unreachable();
}

ircd::string_view
ircd::net::acceptor::handle_alpn(SSL &ssl,
                                 const vector_view<const string_view> &in)
{
	if(empty(in))
		return {};

	log::debug
	{
		log, "%s offered %zu ALPN protocols",
		loghead(*this),
		size(in),
	};

	#ifdef IRCD_NET_ACCEPTOR_DEBUG_ALPN
	for(size_t i(0); i < size(in); ++i)
	{
		log::debug
		{
			log, "%s ALPN protocol %zu of %zu: '%s'",
			loghead(*this),
			i,
			size(in),
			in[i],
		};
	}
	#endif IRCD_NET_ACCEPTOR_DEBUG_ALPN

	return {};
}

static int
ircd_net_acceptor_handle_alpn(SSL *const s,
                              const unsigned char **out,
                              unsigned char *const outlen,
                              const unsigned char *const in,
                              unsigned int inlen,
                              void *const arg)
noexcept try
{
	static const size_t PROTOS_MAX
	{
		8
	};

	auto &acceptor
	{
		*reinterpret_cast<ircd::net::acceptor *>(arg)
	};

	size_t p(0), i(0);
	ircd::string_view protos[PROTOS_MAX];
	while(i < inlen && p < PROTOS_MAX)
	{
		const uint8_t &len(in[i++]);
		if(unlikely(!len || i + len >= inlen))
			break;

		protos[p++] = ircd::string_view
		{
			reinterpret_cast<const char *>(in + i), len
		};

		i += len;
	}

	const ircd::vector_view<const ircd::string_view> vec
	{
		protos, p
	};

	const ircd::string_view sel
	{
		acceptor.handle_alpn(*s, vec)
	};

	if(!sel)
		return SSL_TLSEXT_ERR_NOACK;

	*out = reinterpret_cast<const unsigned char *>(data(sel));
	*outlen = size(sel);
	return SSL_TLSEXT_ERR_OK;
}
catch(const std::exception &)
{
	return SSL_TLSEXT_ERR_ALERT_FATAL;
}
catch(...)
{
	ircd::log::critical
	{
		ircd::net::acceptor::log,
		"Acceptor ALPN callback unhandled."
	};

	ircd::terminate();
	__builtin_unreachable();
}

bool
ircd::net::acceptor::handle_sni(SSL &ssl,
                                int &client_server)
try
{
	const string_view &name
	{
		openssl::server_name(ssl)
	};

	if(!name)
		return true;

	const string_view accept[]
	{
		this->cname,
	};

	const bool accepts
	{
		std::find(begin(accept), end(accept), name) != end(accept)
	};

	if(!accepts)
	{
		log::dwarning
		{
			log, "%s unrecognized SNI '%s' offered.",
			loghead(*this),
			name,
		};

		return false;
	}

	log::debug
	{
		log, "%s offered SNI '%s'",
		loghead(*this),
		name
	};

	return true;
}
catch(const sni_warning &e)
{
	log::warning
	{
		log, "%s during SNI :%s",
		loghead(*this),
		e.what()
	};

	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "%s during SNI :%s",
		loghead(*this),
		e.what()
	};

	throw;
}

static int
ircd_net_acceptor_handle_sni(SSL *const s,
                             int *const i,
                             void *const a)
noexcept try
{
	if(unlikely(!s || !i || !a))
		throw ircd::panic
		{
			"Missing arguments to callback s:%p i:%p a:%p", s, i, a
		};

	auto &acceptor
	{
		*reinterpret_cast<ircd::net::acceptor *>(a)
	};

	return acceptor.handle_sni(*s, *i)?
		SSL_TLSEXT_ERR_OK:
		SSL_TLSEXT_ERR_NOACK;
}
catch(const ircd::net::acceptor::sni_warning &)
{
	return SSL_TLSEXT_ERR_ALERT_WARNING;
}
catch(const std::exception &)
{
	return SSL_TLSEXT_ERR_ALERT_FATAL;
}
catch(...)
{
	ircd::log::critical
	{
		ircd::net::acceptor::log,
		"Acceptor SNI callback unhandled."
	};

	ircd::terminate();
	__builtin_unreachable();
}

void
ircd::net::acceptor::configure(const json::object &opts)
{
	log::debug
	{
		log, "%s preparing listener socket configuration...",
		loghead(*this)
	};

	configure_password(opts);
	configure_flags(opts);
	configure_ciphers(opts);
	configure_curves(opts);
	configure_certs(opts);

	SSL_CTX_set_alpn_select_cb(ssl.native_handle(), ircd_net_acceptor_handle_alpn, this);
	SSL_CTX_set_tlsext_servername_callback(ssl.native_handle(), ircd_net_acceptor_handle_sni);
	SSL_CTX_set_tlsext_servername_arg(ssl.native_handle(), this);
}

void
ircd::net::acceptor::configure_flags(const json::object &opts)
{
	ulong flags(0);

	if(opts.get<bool>("ssl_default_workarounds", false))
		flags |= ssl.default_workarounds;

	if(opts.get<bool>("ssl_single_dh_use", false))
		flags |= ssl.single_dh_use;

	if(opts.get<bool>("ssl_no_sslv2", false))
		flags |= ssl.no_sslv2;

	if(opts.get<bool>("ssl_no_sslv3", false))
		flags |= ssl.no_sslv3;

	if(opts.get<bool>("ssl_no_tlsv1", false))
		flags |= ssl.no_tlsv1;

	if(opts.get<bool>("ssl_no_tlsv1_1", false))
		flags |= ssl.no_tlsv1_1;

	if(opts.get<bool>("ssl_no_tlsv1_2", false))
		flags |= ssl.no_tlsv1_2;

	ssl.set_options(flags);
}

void
ircd::net::acceptor::configure_ciphers(const json::object &opts)
{
	if(!empty(unquote(opts["ssl_cipher_list"])))
	{
		const json::string &list
		{
			opts["ssl_cipher_list"]
		};

		assert(ssl.native_handle());
		openssl::set_cipher_list(*ssl.native_handle(), list);
	}
	else if(!empty(string_view(ssl_cipher_list)))
	{
		assert(ssl.native_handle());
		const string_view &list
		{
			ssl_cipher_list
		};

		openssl::set_cipher_list(*ssl.native_handle(), list);
	}
	else if(!empty(string_view(ssl_cipher_blacklist)))
	{
		assert(ssl.native_handle());

		std::stringstream res;
		const string_view &blacklist
		{
			ssl_cipher_blacklist
		};

		const auto ciphers
		{
			openssl::cipher_list(*ssl.native_handle(), 0)
		};

		ircd::tokens(ciphers, ':', [&res, &blacklist]
		(const string_view &cipher)
		{
			assert(cipher);
			if(!has(blacklist, cipher))
				res << cipher << ':';
		});

		std::string list(res.str());
		assert(list.empty() || list.back() == ':');
		list.pop_back();

		openssl::set_cipher_list(*ssl.native_handle(), list);
	}
}

void
ircd::net::acceptor::configure_curves(const json::object &opts)
{
	if(!empty(unquote(opts["ssl_curve_list"])))
	{
		const json::string &list
		{
			opts["ssl_curve_list"]
		};

		assert(ssl.native_handle());
		openssl::set_curves(*ssl.native_handle(), list);
	}
	else if(!empty(string_view(ssl_curve_list)))
	{
		const string_view &list
		{
			ssl_curve_list
		};

		assert(ssl.native_handle());
		openssl::set_curves(*ssl.native_handle(), list);
	}
}

void
ircd::net::acceptor::configure_certs(const json::object &opts)
{
	if(!empty(unquote(opts["certificate_chain_path"])))
	{
		const json::string filename
		{
			opts["certificate_chain_path"]
		};

		if(!fs::exists(filename))
			throw error
			{
				"%s SSL certificate chain file @ `%s' not found",
				loghead(*this),
				filename
			};

		ssl.use_certificate_chain_file(filename);
		log::info
		{
			log, "%s using certificate chain file '%s'",
			loghead(*this),
			filename
		};
	}

	if(!empty(unquote(opts["certificate_pem_path"])))
	{
		const std::string filename
		{
			unquote(opts.get("certificate_pem_path", name + ".crt"))
		};

		if(!fs::exists(filename))
			throw error
			{
				"%s SSL certificate pem file @ `%s' not found",
				loghead(*this),
				filename
			};

		ssl.use_certificate_file(filename, asio::ssl::context::pem);

		const auto *const x509
		{
			SSL_CTX_get0_certificate(ssl.native_handle())
		};

		this->cname = ircd::string(rfc3986::DOMAIN_BUFSIZE | SHRINK_TO_FIT, [&x509]
		(const mutable_buffer &buf)
		{
			return x509?
				openssl::subject_common_name(buf, *x509):
				string_view{};
		});

		log::info
		{
			log, "%s using file '%s' with certificate for '%s'",
			loghead(*this),
			filename,
			this->cname,
		};
	}

	if(!empty(unquote(opts["private_key_pem_path"])))
	{
		const std::string filename
		{
			unquote(opts.get("private_key_pem_path", name + ".crt.key"))
		};

		if(!fs::exists(filename))
			throw error
			{
				"%s SSL private key file @ `%s' not found",
				loghead(*this),
				filename
			};

		ssl.use_private_key_file(filename, asio::ssl::context::pem);
		log::info
		{
			log, "%s using private key file '%s'",
			loghead(*this),
			filename
		};
	}
}

void
ircd::net::acceptor::configure_dh(const json::object &opts)
{
	if(!empty(unquote(opts["tmp_dh_path"])))
	{
		const json::string filename
		{
			opts.at("tmp_dh_path")
		};

		if(!fs::exists(filename))
			throw error
			{
				"%s SSL tmp dh file @ `%s' not found",
				loghead(*this),
				filename
			};

		ssl.use_tmp_dh_file(filename);
		log::info
		{
			log, "%s using tmp dh file '%s'",
			loghead(*this),
			filename,
		};

		return;
	}

	assert(ssl.native_handle());
	openssl::set_ecdh_auto(*ssl.native_handle(), true);
}

void
ircd::net::acceptor::configure_password(const json::object &opts)
{
	//TODO: XXX
	ssl.set_password_callback([this]
	(const auto &size, const auto &purpose)
	{
		log::notice
		{
			log, "%s asking for password with purpose '%s' (size: %zu)",
			loghead(*this),
			purpose,
			size
		};

		//XXX: TODO
		assert(0);
		return "foobar";
	});
}

//
// acceptor_udp
//

std::ostream &
ircd::net::operator<<(std::ostream &s, const acceptor_udp &a)
{
	s << loghead(a);
	return s;
}

ircd::string_view
ircd::net::loghead(const acceptor_udp &a)
{
	thread_local char buf[512];
	return loghead(buf, a);
}

ircd::string_view
ircd::net::loghead(const mutable_buffer &out,
                   const acceptor_udp &a)
{
	thread_local char addrbuf[128];
	return fmt::sprintf
	{
		out, "[%s] @ [%s]:%u",
		a.name,
		string(addrbuf, a.ep.address()),
		a.ep.port(),
	};
}

//
// acceptor_udp::acceptor
//

ircd::net::acceptor_udp::acceptor_udp(const string_view &name,
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
	make_address(unquote(opts.get("host", "*"_sv))),
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
		log, "%s opened listener socket", loghead(*this)
	};

	a.bind(ep);
	log::debug
	{
		log, "%s bound listener socket", loghead(*this)
	};
}
catch(const boost::system::system_error &e)
{
	throw_system_error(e);
}

ircd::net::acceptor_udp::~acceptor_udp()
noexcept
{
}

void
ircd::net::acceptor_udp::join()
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
ircd::net::acceptor_udp::interrupt()
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
ircd::net::acceptor_udp::operator()(datagram &datagram)
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
ircd::net::acceptor_udp::flags(const flag &flag)
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
:remote{remote}
,flag{flag}
{
	cbuf = buf;
	cbufs =
	{
		&this->cbuf, 1
	};
}

ircd::net::listener_udp::datagram::datagram(const mutable_buffer &buf,
                                            const enum flag &flag)
:flag{flag}
{
	mbuf = buf;
	mbufs =
	{
		&this->mbuf, 1
	};
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
		log, "socket(%p) close: %s",
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
	log::debug
	{
		log, "socket:%lu attempting connect remote[%s] to:%ld$ms",
		this->id,
		string(ep),
		opts.connect_timeout.count()
	};

	static ios::descriptor desc
	{
		"ircd::net::socket connect"
	};

	auto connect_handler
	{
		std::bind(&socket::handle_connect, this, weak_from(*this), opts, std::move(callback), ph::_1)
	};

	set_timeout(opts.connect_timeout);
	sd.async_connect(ep, ios::handle(desc, std::move(connect_handler)));
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

	static ios::descriptor desc
	{
		"ircd::net::socket handshake"
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
	ssl.async_handshake(handshake_type::client, ios::handle(desc, std::move(handshake_handler)));
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
			static ios::descriptor desc
			{
				"ircd::net::socket shutdown"
			};

			auto disconnect_handler
			{
				std::bind(&socket::handle_disconnect, this, shared_from(*this), std::move(callback), ph::_1)
			};

			set_timeout(opts.timeout);
			ssl.async_shutdown(ios::handle(desc, std::move(disconnect_handler)));
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

bool
ircd::net::socket::cancel()
noexcept
{
	cancel_timeout();

	boost::system::error_code ec;
	sd.cancel(ec);
	if(unlikely(ec))
	{
		thread_local char ecbuf[64];
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
	static ios::descriptor desc[4]
	{
		{ "ircd::net::socket::wait ready::ANY"   },
		{ "ircd::net::socket::wait ready::READ"  },
		{ "ircd::net::socket::wait ready::WRITE" },
		{ "ircd::net::socket::wait ready::ERROR" },
	};

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
				ircd::post(desc[1], [handle(std::move(handle))]
				{
					handle(error_code{});
				});

				return;
			}

			// The problem here is that the wait operation gives ec=success on both a
			// socket error and when data is actually available. We then have to check
			// using a non-blocking peek in the handler. By doing it this way here we
			// just get the error in the handler's ec.
			//sd.async_wait(bufs, sd.message_peek, ios::handle(desc[1], [handle(std::move(handle))]
			sd.async_receive(bufs, sd.message_peek, ios::handle(desc[1], [handle(std::move(handle))]
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
			sd.async_wait(wait_type::wait_write, ios::handle(desc[2], std::move(handle)));
			return;
		}

		case ready::ERROR:
		{
			sd.async_wait(wait_type::wait_error, ios::handle(desc[3], std::move(handle)));
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

	thread_local char ecbuf[64];
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
                                  const open_opts &opts,
                                  eptr_handler callback,
                                  error_code ec)
noexcept try
{
	using std::errc;

	const life_guard<socket> s{wp};
	thread_local char ecbuf[64], epbuf[128];
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

	thread_local char ecbuf[64];
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

	#ifdef RB_DEBUG
	const auto *const current_cipher
	{
		!ec?
			openssl::current_cipher(*this):
			nullptr
	};

	thread_local char ecbuf[64];
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
		log, "verify[%s]: %s",
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

	static ios::descriptor descriptor
	{
		"ircd::net::socket timer"
	};

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
	timer.async_wait(ios::handle(descriptor, std::move(handler)));
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
	thread_local char buf[128];
	const critical_assertion ca;
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
		consume(out, copy(out, "["_sv));

	if(ipp)
		consume(out, size(string(out, std::get<ipport::IP>(ipp))));

	if(need_bracket)
		consume(out, copy(out, "]"_sv));

	if(has_port)
	{
		consume(out, copy(out, ":"_sv));
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
	thread_local char buf[128];
	const critical_assertion ca;
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
	thread_local char tlbuf[256];

	if(net::port(hp) == 0 || net::port(hp) == port)
		return fmt::sprintf
		{
			buf, "%s",
			tolower(tlbuf, host(hp)),
		};

	return fmt::sprintf
	{
		buf, "%s:%u",
		tolower(tlbuf, host(hp)),
		net::port(hp)
	};
}

ircd::string_view
ircd::net::string(const mutable_buffer &buf,
                  const hostport &hp)
{
	thread_local char tlbuf[2][256];

	if(empty(service(hp)) && port(hp) == 0)
		return fmt::sprintf
		{
			buf, "%s",
			tolower(tlbuf[0], host(hp)),
		};

	if(!empty(service(hp)))
		return fmt::sprintf
		{
			buf, "%s:%s",
			tolower(tlbuf[0], host(hp)),
			tolower(tlbuf[1], service(hp)),
		};

	if(empty(service(hp)) && port(hp) != 0)
		return fmt::sprintf
		{
			buf, "%s:%u",
			tolower(tlbuf[0], host(hp)),
			port(hp)
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
