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

#ifdef __cplusplus
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include "bufs.h"
#include "ctx_ctx.h"

namespace ircd {

namespace ip = boost::asio::ip;
using boost::system::error_code;
using boost::asio::steady_timer;

struct sock
:std::enable_shared_from_this<sock>
{
	using message_flags = boost::asio::socket_base::message_flags;

	ip::tcp::socket sd;
	steady_timer timer;
	bool timedout;

	operator const ip::tcp::socket &() const     { return sd;                                      }
	operator ip::tcp::socket &()                 { return sd;                                      }
	ip::tcp::endpoint remote() const             { return sd.remote_endpoint();                    }
	ip::tcp::endpoint local() const              { return sd.local_endpoint();                     }

	void handle_timeout(const std::weak_ptr<sock>, const error_code &);
	template<class duration> void set_timeout(const duration &);

	template<class mutable_buffers> auto recv_some(const mutable_buffers &, const message_flags & = 0);
	template<class mutable_buffers> auto recv(const mutable_buffers &);

	template<class const_buffers> auto send_some(const const_buffers &, const message_flags & = 0);
	template<class const_buffers> auto send(const const_buffers &);

	sock(boost::asio::io_service *const &ios  = ircd::ios);
};

ip::address remote_address(const sock &);
std::string remote_ip(const sock &);
uint16_t remote_port(const sock &);
ip::address local_address(const sock &);
std::string local_ip(const sock &);
uint16_t local_port(const sock &);

inline
sock::sock(boost::asio::io_service *const &ios)
:sd{*ios}
,timer{*ios}
{
}

// Block until entirely transmitted
template<class const_buffers>
auto
sock::send(const const_buffers &bufs)
{
	const auto ret(async_write(sd, bufs, yield(continuation())));
	timer.cancel();
	return ret;
}

// Block until something transmitted, returns amount
template<class const_buffers>
auto
sock::send_some(const const_buffers &bufs,
                const message_flags &flags)
{
	return sd.async_send(bufs, flags, yield(continuation()));
}

// Block until the buffers are completely full
template<class mutable_buffers>
auto
sock::recv(const mutable_buffers &bufs)
{
	const auto ret(async_read(sd, bufs, yield(continuation())));
	timer.cancel();
	return ret;
}

// Block until something in buffers, returns size
template<class mutable_buffers>
auto
sock::recv_some(const mutable_buffers &bufs,
                const message_flags &flags)
{
	return sd.async_receive(bufs, flags, yield(continuation()));
}

template<class duration>
void
sock::set_timeout(const duration &t)
{
	if(t < duration(0))
		return;

	timer.expires_from_now(t);
	timer.async_wait(std::bind(&sock::handle_timeout, this, shared_from_this(), ph::_1));
}

inline void
sock::handle_timeout(const std::weak_ptr<sock> wp,
                     const error_code &ec)
{
	using namespace boost::system::errc;

	if(!wp.expired()) switch(ec.value())
	{
		case success:
		{
			timedout = true;
			error_code sd_ec;
			sd.cancel(sd_ec);
			return;
		}

		case operation_canceled:
			timedout = false;
			return;

		default:
			log::error("sock::handle_timeout(): unexpected: %s\n", ec.message().c_str());
			return;
	}
}

inline uint16_t
local_port(const sock &sock)
{
	return sock.local().port();
}

inline std::string
local_ip(const sock &sock)
{
	return local_address(sock).to_string();
}

inline ip::address
local_address(const sock &sock)
{
	return sock.local().address();
}

inline uint16_t
remote_port(const sock &sock)
{
	return sock.remote().port();
}

inline std::string
remote_ip(const sock &sock)
{
	return remote_address(sock).to_string();
}

inline ip::address
remote_address(const sock &sock)
{
	return sock.remote().address();
}

}      // namespace ircd
#endif // __cplusplus
