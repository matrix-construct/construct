/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
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

#pragma once
#define HAVE_IRCD_NET_SOCKET_H

// This file is not included with the IRCd standard include stack because
// it requires symbols we can't forward declare without boost headers. It
// is part of the <ircd/asio.h> stack which can be included in your
// definition file if you need low level access to this socket API. The
// client.h still offers higher level access to sockets without requiring
// boost headers; please check that for satisfaction before including this.

namespace ircd::net
{
	struct socket;

	extern asio::ssl::context sslv23_client;

	std::shared_ptr<socket> connect(const ip::tcp::endpoint &remote, const milliseconds &timeout);
}

struct ircd::net::socket
:std::enable_shared_from_this<ircd::net::socket>
{
	struct io;
	struct stat;
	struct xfer;
	struct scope_timeout;

	using endpoint = ip::tcp::endpoint;
	using wait_type = ip::tcp::socket::wait_type;
	using message_flags = asio::socket_base::message_flags;
	using handshake_type = asio::ssl::stream<ip::tcp::socket>::handshake_type;
	using handler = std::function<void (const error_code &) noexcept>;
	using xfer_handler = std::function<void (xfer) noexcept>;

	struct stat
	{
		size_t bytes {0};
		size_t calls {0};
	};

	ip::tcp::socket sd;
	asio::ssl::stream<ip::tcp::socket &> ssl;
	steady_timer timer;
	stat in, out;
	bool timedout {false};

	void call_user(const handler &, const error_code &ec) noexcept;
	void handle_timeout(std::weak_ptr<socket> wp, const error_code &ec) noexcept;
	void handle_handshake(std::weak_ptr<socket> wp, handler, const error_code &ec) noexcept;
	void handle_connect(std::weak_ptr<socket> wp, handler, const error_code &ec) noexcept;
	void handle(std::weak_ptr<socket>, handler, const error_code &) noexcept;

  public:
	operator const ip::tcp::socket &() const     { return sd;                                      }
	operator ip::tcp::socket &()                 { return sd;                                      }
	operator const SSL &() const;
	operator SSL &();

	bool connected() const noexcept;             // false on any sock errs
	endpoint remote() const;                     // getpeername(); throws if not conn
	endpoint local() const;                      // getsockname(); throws if not conn/bound
	size_t available() const;                    // throws on errors; use friend variant for noex..
	size_t readable() const;                     // throws on errors; ioctl
	size_t rbufsz() const;                       // throws on errors; SO_RCVBUF
	size_t wbufsz() const;                       // throws on errors; SO_SNDBUF
	bool blocking() const;                       // throws on errors;

	void rbufsz(const size_t &);                 // throws; set SO_RCVBUF bytes
	void wbufsz(const size_t &);                 // throws; set SO_RCVBUF bytes
	void blocking(const bool &);                 // throws; set blocking

	// low level read suite
	template<class iov> auto read_some(const iov &, xfer_handler);
	template<class iov> auto read_some(const iov &);
	template<class iov> auto read(const iov &, xfer_handler);
	template<class iov> auto read(const iov &);

	// low level write suite
	template<class iov> auto write_some(const iov &, xfer_handler);
	template<class iov> auto write_some(const iov &);
	template<class iov> auto write(const iov &, xfer_handler);
	template<class iov> auto write(const iov &);

	// Timer for this socket
	void set_timeout(const milliseconds &, handler);
	void set_timeout(const milliseconds &);
	milliseconds cancel_timeout() noexcept;

	// Asynchronous callback when socket ready
	void operator()(const wait_type &, const milliseconds &timeout, handler);
	void operator()(const wait_type &, handler);
	bool cancel() noexcept;

	// SSL handshake after connect (untimed)
	void handshake(const handshake_type &, handler callback);
	void handshake(const handshake_type & = handshake_type::client);

	// Connect to host (untimed)
	void connect(const endpoint &ep, handler callback);
	void connect(const endpoint &ep);

	// Connect to host and handshake composit (timed)
	void open(const endpoint &ep, const milliseconds &timeout, handler callback);
	void open(const endpoint &ep, const milliseconds &timeout);

	bool disconnect(const dc &type);

	socket(asio::ssl::context &ssl               = sslv23_client,
	       boost::asio::io_service *const &ios   = ircd::ios);

	// Socket cannot be copied or moved; must be constructed as shared ptr
	socket(socket &&) = delete;
	socket(const socket &) = delete;
	~socket() noexcept;
};

class ircd::net::socket::scope_timeout
{
	socket *s {nullptr};

  public:
	bool cancel() noexcept;   // invoke timer.cancel() before dtor
	bool release();           // cancels the cancel;

	scope_timeout(socket &, const milliseconds &timeout, socket::handler handler);
	scope_timeout(socket &, const milliseconds &timeout);
	scope_timeout() = default;
	scope_timeout(scope_timeout &&) noexcept;
	scope_timeout(const scope_timeout &) = delete;
	scope_timeout &operator=(scope_timeout &&) noexcept;
	scope_timeout &operator=(const scope_timeout &) = delete;
	~scope_timeout() noexcept;
};

class ircd::net::socket::io
{
	struct socket &sock;
	struct stat &stat;
	size_t bytes;

  public:
	operator size_t() const;

	io(struct socket &, struct stat &, const size_t &bytes);
	io(struct socket &, struct stat &, const std::function<size_t ()> &closure);
};

struct ircd::net::socket::xfer
{
	std::exception_ptr eptr;
	size_t bytes {0};

	xfer(const error_code &ec = {}, const size_t &bytes = 0);
};

template<class iov>
auto
ircd::net::socket::write(const iov &bufs)
{
	return io{*this, out, [&]
	{
		return async_write(ssl, bufs, asio::transfer_all(), yield_context{to_asio{}});
	}};
}

template<class iov>
auto
ircd::net::socket::write(const iov &bufs,
                         xfer_handler handler)
{
	async_write(ssl, bufs, asio::transfer_all(), [this, handler(std::move(handler))]
	(const error_code &ec, const size_t &bytes)
	noexcept
	{
		io{*this, out, bytes};
		handler(xfer{ec, bytes});
	});
}

template<class iov>
auto
ircd::net::socket::write_some(const iov &bufs)
{
	return io{*this, out, [&]
	{
		return ssl.async_write_some(bufs, yield_context{to_asio{}});
	}};
}

template<class iov>
auto
ircd::net::socket::write_some(const iov &bufs,
                              xfer_handler handler)
{
	ssl.async_write_some(bufs, [this, handler(std::move(handler))]
	(const error_code &ec, const size_t &bytes)
	noexcept
	{
		io{*this, out, bytes};
		handler(xfer{ec, bytes});
	});
}

template<class iov>
auto
ircd::net::socket::read(const iov &bufs)
{
	return io{*this, in, [&]
	{
		const size_t ret
		{
			async_read(ssl, bufs, yield_context{to_asio{}})
		};

		if(unlikely(!ret))
			throw boost::system::system_error(boost::asio::error::eof);

		return ret;
	}};
}

template<class iov>
auto
ircd::net::socket::read(const iov &bufs,
                        xfer_handler handler)
{
	async_read(ssl, bufs, [this, handler(std::move(handler))]
	(const error_code &ec, const size_t &bytes)
	noexcept
	{
		io{*this, in, bytes};
		handler(xfer{ec, bytes});
	});
}

template<class iov>
auto
ircd::net::socket::read_some(const iov &bufs)
{
	return io{*this, in, [&]
	{
		const size_t ret
		{
			ssl.async_read_some(bufs, yield_context{to_asio{}})
		};

		if(unlikely(!ret))
			throw boost::system::system_error(boost::asio::error::eof);

		return ret;
	}};
}

template<class iov>
auto
ircd::net::socket::read_some(const iov &bufs,
                             xfer_handler handler)
{
	ssl.async_read_some(bufs, [this, handler(std::move(handler))]
	(const error_code &ec, const size_t &bytes)
	noexcept
	{
		io{*this, in, bytes};
		handler(xfer{ec, bytes});
	});
}
