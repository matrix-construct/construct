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

/// This file is not included with the IRCd standard include stack because
/// it requires symbols we can't forward declare without boost headers. It
/// is part of the <ircd/asio.h> stack which can be included in your
/// definition file if you need low level access to this socket API. The
/// client.h still offers higher level access to sockets without requiring
/// boost headers; please check that for satisfaction before including this.

namespace ircd::net
{
	namespace ip = asio::ip;

	using boost::system::error_code;
	using asio::steady_timer;

	IRCD_EXCEPTION(error, nxdomain)

	struct socket;

	extern asio::ssl::context sslv23_client;

	std::string string(const ip::address &);
	std::string string(const ip::tcp::endpoint &);
	ip::address address(const ip::tcp::endpoint &);
	std::string hostaddr(const ip::tcp::endpoint &);
	uint16_t port(const ip::tcp::endpoint &);
}

namespace ircd
{
	using net::error_code;
	using net::string;
	using net::address;
	using net::hostaddr;
	using net::port;
}

struct ircd::net::socket
:std::enable_shared_from_this<ircd::net::socket>
{
	struct stat;
	struct scope_timeout;
	struct io;

	enum dc
	{
		RST,                // hardest disconnect
		FIN,                // graceful shutdown both directions
		FIN_SEND,           // graceful shutdown send side
		FIN_RECV,           // graceful shutdown recv side
		SSL_NOTIFY,         // SSL close_notify (async, errors ignored)
		SSL_NOTIFY_YIELD,   // SSL close_notify (yields context, throws)
	};

	struct stat
	{
		size_t bytes {0};
		size_t calls {0};
	};

	using message_flags = boost::asio::socket_base::message_flags;
	using handshake_type = asio::ssl::stream<ip::tcp::socket>::handshake_type;
	using handler = std::function<void (const error_code &) noexcept>;

	asio::ssl::stream<ip::tcp::socket> ssl;
	ip::tcp::socket &sd;
	steady_timer timer;
	stat in, out;
	bool timedout;

	void call_user(const handler &, const error_code &) noexcept;
	bool handle_error(const error_code &ec);
	void handle_timeout(std::weak_ptr<socket> wp, const error_code &ec) noexcept;
	void handle(std::weak_ptr<socket>, handler, const error_code &, const size_t &) noexcept;

  public:
	operator const ip::tcp::socket &() const     { return sd;                                      }
	operator ip::tcp::socket &()                 { return sd;                                      }

	ip::tcp::endpoint remote() const             { return sd.remote_endpoint();                    }
	ip::tcp::endpoint local() const              { return sd.local_endpoint();                     }

	bool connected() const noexcept;
	size_t available() const;

	template<class iov> auto read_some(const iov &, error_code &);
	template<class iov> auto read_some(const iov &);
	template<class iov> auto read(const iov &, error_code &);
	template<class iov> auto read(const iov &);

	template<class iov> auto write_some(const iov &, error_code &);
	template<class iov> auto write_some(const iov &);
	template<class iov> auto write(const iov &, error_code &);
	template<class iov> auto write(const iov &);

	void set_timeout(const milliseconds &, handler);
	void set_timeout(const milliseconds &);

	// Asynchronous 'ready' closure
	void operator()(const milliseconds &timeout, handler);
	void operator()(handler);
	bool cancel() noexcept;

	void disconnect(const dc &type = dc::SSL_NOTIFY);
	void connect(const ip::tcp::endpoint &ep, const milliseconds &timeout = -1ms);

	socket(const std::string &host,
	       const uint16_t &port,
	       const milliseconds &timeout           = -1ms,
	       asio::ssl::context &ssl               = sslv23_client,
	       boost::asio::io_service *const &ios   = ircd::ios);

	socket(const ip::tcp::endpoint &remote,
	       const milliseconds &timeout           = -1ms,
	       asio::ssl::context &ssl               = sslv23_client,
	       boost::asio::io_service *const &ios   = ircd::ios);

	socket(asio::ssl::context &ssl               = sslv23_client,
	       boost::asio::io_service *const &ios   = ircd::ios);

	socket(socket &&) = delete;
	socket(const socket &) = delete;
	~socket() noexcept;
};

class ircd::net::socket::scope_timeout
{
	socket *s;

  public:
	scope_timeout(socket &, const milliseconds &timeout, const socket::handler &handler);
	scope_timeout(socket &, const milliseconds &timeout);
	scope_timeout(const scope_timeout &) = delete;
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

	io(struct socket &, struct stat &, const std::function<size_t ()> &closure);
};

template<class iov>
auto
ircd::net::socket::write(const iov &bufs)
{
	return io(*this, out, [&]
	{
		return async_write(ssl, bufs, asio::transfer_all(), yield_context{to_asio{}});
	});
}

template<class iov>
auto
ircd::net::socket::write(const iov &bufs,
                         error_code &ec)
{
	return io(*this, out, [&]
	{
		return async_write(ssl, bufs, asio::transfer_all(), yield_context{to_asio{}}[ec]);
	});
}

template<class iov>
auto
ircd::net::socket::write_some(const iov &bufs)
{
	return io(*this, out, [&]
	{
		return ssl.async_write_some(bufs, yield_context{to_asio{}});
	});
}

template<class iov>
auto
ircd::net::socket::write_some(const iov &bufs,
                              error_code &ec)
{
	return io(*this, out, [&]
	{
		return ssl.async_write_some(bufs, yield_context{to_asio{}}[ec]);
	});
}

template<class iov>
auto
ircd::net::socket::read(const iov &bufs)
{
	return io(*this, in, [&]
	{
		const size_t ret(async_read(ssl, bufs, yield_context{to_asio{}}));

		if(unlikely(!ret))
			throw boost::system::system_error(boost::asio::error::eof);

		return ret;
	});
}

template<class iov>
auto
ircd::net::socket::read(const iov &bufs,
                        error_code &ec)
{
	return io(*this, in, [&]
	{
		return async_read(ssl, bufs, yield_context{to_asio{}}[ec]);
	});
}

template<class iov>
auto
ircd::net::socket::read_some(const iov &bufs)
{
	return io(*this, in, [&]
	{
		const size_t ret(ssl.async_read_some(bufs, yield_context{to_asio{}}));

		if(unlikely(!ret))
			throw boost::system::system_error(boost::asio::error::eof);

		return ret;
	});
}

template<class iov>
auto
ircd::net::socket::read_some(const iov &bufs,
                             error_code &ec)
{
	return io(*this, in, [&]
	{
		return ssl.async_read_some(bufs, yield_context{to_asio{}}[ec]);
	});
}
