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
#define HAVE_IRCD_CLIENT_SOCK_H

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include "ctx/continuation.h"

namespace ircd {

namespace asio = boost::asio;
namespace ip = asio::ip;

using boost::system::error_code;
using asio::steady_timer;

IRCD_EXCEPTION(error, nxdomain)

extern asio::ssl::context sslv23_client;

} // namespace ircd

namespace ircd {

struct socket
:std::enable_shared_from_this<socket>
{
	struct init;
	struct stat;
	struct scope_timeout;
	struct io;

	enum dc
	{
		RST,          // hardest disconnect
		FIN,          // graceful shutdown both directions
		FIN_SEND,     // graceful shutdown send side
		FIN_RECV,     // graceful shutdown recv side
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

	void handle_timeout(const std::weak_ptr<socket> wp, const error_code &ec) noexcept;
	bool handle_ready(const error_code &ec) noexcept;

  public:
	operator const ip::tcp::socket &() const     { return sd;                                      }
	operator ip::tcp::socket &()                 { return sd;                                      }

	ip::tcp::endpoint remote() const             { return sd.remote_endpoint();                    }
	ip::tcp::endpoint local() const              { return sd.local_endpoint();                     }

	bool connected() const noexcept;
	auto available() const                       { return sd.available();                          }

	template<class duration> void set_timeout(const duration &, handler);
	template<class duration> void set_timeout(const duration &);

	template<class iov> auto read_some(const iov &, error_code &);
	template<class iov> auto read_some(const iov &);
	template<class iov> auto read(const iov &, error_code &);
	template<class iov> auto read(const iov &);

	template<class iov> auto write_some(const iov &, error_code &);
	template<class iov> auto write_some(const iov &);
	template<class iov> auto write(const iov &, error_code &);
	template<class iov> auto write(const iov &);

	// Asynchronous 'ready' closure
	void operator()(const milliseconds &timeout, handler);
	void operator()(handler);
	void cancel();

	void disconnect(const dc &type = dc::FIN);
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

	~socket() noexcept;
};

class socket::scope_timeout
{
	socket *s;

  public:
	scope_timeout(socket &, const milliseconds &timeout, const socket::handler &handler);
	scope_timeout(socket &, const milliseconds &timeout);
	scope_timeout(const scope_timeout &) = delete;
	scope_timeout &operator=(const scope_timeout &) = delete;
	~scope_timeout() noexcept;
};

class socket::io
{
	struct socket &sock;
	struct stat &stat;
	size_t bytes;

  public:
	operator size_t() const;

	io(struct socket &, struct stat &, const std::function<size_t ()> &closure);
};

struct socket::init
{
	init();
	~init() noexcept;
};

char *read(socket &, char *&start, char *const &stop);
string_view readline(socket &, char *&start, char *const &stop);

size_t write(socket &, const char *const &buf, const size_t &max);
size_t write(socket &, const string_view &);

ip::address remote_address(const socket &);
std::string remote_ip(const socket &);
uint16_t remote_port(const socket &);
ip::address local_address(const socket &);
std::string local_ip(const socket &);
uint16_t local_port(const socket &);


inline
socket::io::io(struct socket &sock,
               struct stat &stat,
               const std::function<size_t ()> &closure)
:sock{sock}
,stat{stat}
,bytes{closure()}
{
	stat.bytes += bytes;
	stat.calls++;
}

inline
socket::io::operator size_t()
const
{
	return bytes;
}

template<class iov>
auto
socket::write(const iov &bufs)
{
	const auto ret(async_write(ssl, bufs, yield(continuation())));
	return ret;
}

template<class iov>
auto
socket::write(const iov &bufs,
              error_code &ec)
{
	const auto ret(async_write(ssl, bufs, yield(continuation())[ec]));
	return ret;
}

template<class iov>
auto
socket::write_some(const iov &bufs)
{
	const auto ret(ssl.async_write_some(bufs, yield(continuation())));
	return ret;
}

template<class iov>
auto
socket::write_some(const iov &bufs,
                   error_code &ec)
{
	const auto ret(ssl.async_write_some(bufs, yield(continuation())[ec]));
	return ret;
}

template<class iov>
auto
socket::read(const iov &bufs)
{
	return io(*this, in, [&]
	{
		const auto ret(async_read(ssl, bufs, yield(continuation())));

		if(unlikely(!ret))
			throw boost::system::system_error(boost::asio::error::eof);

		return ret;
	});
}

template<class iov>
auto
socket::read(const iov &bufs,
             error_code &ec)
{
	return io(*this, in, [&]
	{
		return async_read(ssl, bufs, yield(continuation())[ec]);
	});
}

template<class iov>
auto
socket::read_some(const iov &bufs)
{
	return io(*this, in, [&]
	{
		const auto ret(ssl.async_read_some(bufs, yield(continuation())));

		if(unlikely(!ret))
			throw boost::system::system_error(boost::asio::error::eof);

		return ret;
	});
}

template<class iov>
auto
socket::read_some(const iov &bufs,
                  error_code &ec)
{
	return io(*this, in, [&]
	{
		return ssl.async_read_some(bufs, yield(continuation())[ec]);
	});
}

template<class duration>
void
socket::set_timeout(const duration &t)
{
	if(t < duration(0))
		return;

	timer.expires_from_now(t);
	timer.async_wait(std::bind(&socket::handle_timeout, this, weak_from(*this), ph::_1));
}

template<class duration>
void
socket::set_timeout(const duration &t,
                    handler h)
{
	if(t < duration(0))
		return;

	timer.expires_from_now(t);
	timer.async_wait(std::move(h));
}

inline uint16_t
local_port(const socket &socket)
{
	return socket.local().port();
}

inline std::string
local_ip(const socket &socket)
{
	return local_address(socket).to_string();
}

inline ip::address
local_address(const socket &socket)
{
	return socket.local().address();
}

inline uint16_t
remote_port(const socket &socket)
{
	return socket.remote().port();
}

inline std::string
remote_ip(const socket &socket)
{
	return remote_address(socket).to_string();
}

inline ip::address
remote_address(const socket &socket)
{
	return socket.remote().address();
}

} // namespace ircd

inline
ircd::buffer::mutable_buffer::operator boost::asio::mutable_buffer()
const
{
	return boost::asio::mutable_buffer
	{
		data(*this), size(*this)
	};
}

inline
ircd::buffer::const_buffer::operator boost::asio::const_buffer()
const
{
	return boost::asio::const_buffer
	{
		data(*this), size(*this)
	};
}
