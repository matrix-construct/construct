// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_NET_SOCKET_H

// This file is not included with the IRCd standard include stack because
// it requires symbols we can't forward declare without boost headers. It
// is part of the <ircd/asio.h> stack which can be included in your
// definition file if you need low level access to this socket API.

namespace ircd::net
{
	extern asio::ssl::context sslv23_client;
}

/// Internal socket interface
///
struct ircd::net::socket
:std::enable_shared_from_this<ircd::net::socket>
{
	struct io;
	struct stat;
	struct xfer;

	using endpoint = ip::tcp::endpoint;
	using wait_type = ip::tcp::socket::wait_type;
	using message_flags = asio::socket_base::message_flags;
	using handshake_type = asio::ssl::stream<ip::tcp::socket>::handshake_type;
	using ec_handler = std::function<void (const error_code &)>;
	using eptr_handler = std::function<void (std::exception_ptr)>;

	struct stat
	{
		size_t bytes {0};
		size_t calls {0};
	};

	static uint64_t count;                       // monotonic
	static uint64_t instances;                   // current socket count

	ip::tcp::socket sd;
	asio::ssl::stream<ip::tcp::socket &> ssl;
	stat in, out;
	steady_timer timer;
	uint64_t timer_sem[2] {0};                   // handler, sender
	bool timer_set {false};                      // boolean lockout
	bool timedout {false};
	bool fini {false};

	void call_user(const eptr_handler &, const error_code &) noexcept;
	void call_user(const ec_handler &, const error_code &) noexcept;
	bool handle_verify(bool, asio::ssl::verify_context &, const open_opts &) noexcept;
	void handle_disconnect(std::shared_ptr<socket>, eptr_handler, error_code) noexcept;
	void handle_handshake(std::weak_ptr<socket>, eptr_handler, error_code) noexcept;
	void handle_connect(std::weak_ptr<socket>, open_opts, eptr_handler, error_code) noexcept;
	void handle_timeout(std::weak_ptr<socket>, ec_handler, error_code) noexcept;
	void handle_ready(std::weak_ptr<socket>, ready, ec_handler, error_code, size_t) noexcept;

  public:
	operator const ip::tcp::socket &() const     { return sd;                                      }
	operator ip::tcp::socket &()                 { return sd;                                      }
	operator const SSL &() const;
	operator SSL &();

	endpoint remote() const;                     // getpeername(); throws if not conn
	endpoint local() const;                      // getsockname(); throws if not conn/bound

	// Timer for this socket
	void set_timeout(const milliseconds &, ec_handler);
	void set_timeout(const milliseconds &);
	milliseconds cancel_timeout() noexcept;

	// low level write suite
	template<class iov> size_t write_one(iov&&); // non-blocking
	template<class iov> size_t write_any(iov&&); // non-blocking
	template<class iov> size_t write_few(iov&&); // yielding
	template<class iov> size_t write_all(iov&&); // yielding

	// low level read suite
	template<class iov> size_t read_one(iov&&);  // non-blocking
	template<class iov> size_t read_any(iov&&);  // non-blocking
	template<class iov> size_t read_few(iov&&);  // yielding
	template<class iov> size_t read_all(iov&&);  // yielding

	// low level wait suite
	void wait(const wait_opts &);
	void wait(const wait_opts &, wait_callback_ec);
	void wait(const wait_opts &, wait_callback_eptr);
	void cancel() noexcept;

	// Alias to wait()
	template<class... args> auto operator()(args&&...);

	void disconnect(const close_opts &, eptr_handler);
	void handshake(const open_opts &, eptr_handler);
	void connect(const endpoint &, const open_opts &, eptr_handler);

	socket(asio::ssl::context &ssl               = sslv23_client,
	       boost::asio::io_service *const &ios   = ircd::ios);

	// Socket cannot be copied or moved; must be constructed as shared ptr
	socket(socket &&) = delete;
	socket(const socket &) = delete;
	socket &operator=(socket &&) = delete;
	socket &operator=(const socket &) = delete;
	~socket() noexcept;
};

template<class... args>
auto
ircd::net::socket::operator()(args&&... a)
{
	return this->wait(std::forward<args>(a)...);
}

/// Yields ircd::ctx until buffers are full.
template<class iov>
size_t
ircd::net::socket::read_all(iov&& bufs)
{
	static const auto completion
	{
		asio::transfer_all()
	};

	const auto interruption{[this]
	(ctx::ctx *const &) noexcept
	{
		this->cancel();
	}};

	const size_t ret
	{
		asio::async_read(ssl, std::forward<iov>(bufs), completion, yield_context
		{
			to_asio{interruption}
		})
	};

	if(!ret)
		throw boost::system::system_error
		{
			boost::asio::error::eof
		};

	in.bytes += ret;
	++in.calls;
	return ret;
}

/// Yields ircd::ctx until remote has sent at least some data.
template<class iov>
size_t
ircd::net::socket::read_few(iov&& bufs)
{
	const auto interruption{[this]
	(ctx::ctx *const &) noexcept
	{
		this->cancel();
	}};

	const size_t ret
	{
		ssl.async_read_some(std::forward<iov>(bufs), yield_context
		{
			to_asio{interruption}
		})
	};

	if(!ret)
		throw boost::system::system_error
		{
			boost::asio::error::eof
		};

	in.bytes += ret;
	++in.calls;
	return ret;
}

/// Non-blocking; as much as possible without blocking
template<class iov>
size_t
ircd::net::socket::read_any(iov&& bufs)
{
	assert(!blocking(*this));
	static const auto completion
	{
		asio::transfer_all()
	};

	const size_t ret
	{
		asio::read(ssl, std::forward<iov>(bufs), completion)
	};

	in.bytes += ret;
	++in.calls;
	return ret;
}

/// Non-blocking; One system call only; never throws eof;
template<class iov>
size_t
ircd::net::socket::read_one(iov&& bufs)
{
	assert(!blocking(*this));
	const size_t ret
	{
		ssl.read_some(std::forward<iov>(bufs))
	};

	in.bytes += ret;
	++in.calls;
	return ret;
}

/// Yields ircd::ctx until all buffers are sent.
template<class iov>
size_t
ircd::net::socket::write_all(iov&& bufs)
{
	static const auto completion
	{
		asio::transfer_all()
	};

	const auto interruption{[this]
	(ctx::ctx *const &) noexcept
	{
		this->cancel();
	}};

	const size_t ret
	{
		asio::async_write(ssl, std::forward<iov>(bufs), completion, yield_context
		{
			to_asio{interruption}
		})
	};

	out.bytes += ret;
	++out.calls;
	return ret;
}

/// Yields ircd::ctx until one or more bytes are sent.
template<class iov>
size_t
ircd::net::socket::write_few(iov&& bufs)
{
	const auto interruption{[this]
	(ctx::ctx *const &) noexcept
	{
		this->cancel();
	}};

	const size_t ret
	{
		ssl.async_write_some(std::forward<iov>(bufs), yield_context
		{
			to_asio{interruption}
		})
	};

	out.bytes += ret;
	++out.calls;
	return ret;
}

/// Non-blocking; writes as much as possible without blocking
template<class iov>
size_t
ircd::net::socket::write_any(iov&& bufs)
{
	static const auto completion
	{
		asio::transfer_all()
	};

	assert(!blocking(*this));
	const size_t ret
	{
		asio::write(ssl, std::forward<iov>(bufs), completion)
	};

	out.bytes += ret;
	++out.calls;
	return ret;
}

/// Non-blocking; Writes one "unit" of data or less; never more.
template<class iov>
size_t
ircd::net::socket::write_one(iov&& bufs)
{
	assert(!blocking(*this));
	const size_t ret
	{
		ssl.write_some(std::forward<iov>(bufs))
	};

	out.bytes += ret;
	++out.calls;
	return ret;
}
