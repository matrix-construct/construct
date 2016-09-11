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

namespace ircd   {

namespace ip = boost::asio::ip;
using boost::system::error_code;
using boost::asio::steady_timer;

struct sock
{
	ip::tcp::socket sd;
	steady_timer timer;
	std::exception_ptr eptr;
	std::array<char, BUFSIZE> rbuf alignas(16);
	uint16_t checked;
	uint16_t length;
	tape reel;

	operator const ip::tcp::socket &() const     { return sd;                                      }
	operator ip::tcp::socket &()                 { return sd;                                      }
	ip::tcp::endpoint remote() const             { return sd.remote_endpoint();                    }
	ip::tcp::endpoint local() const              { return sd.local_endpoint();                     }

	bool terminated() const;
	uint16_t remaining() const;
	size_t handle_pck(const error_code &, const size_t) noexcept;

	sock(boost::asio::io_service *const &ios  = ircd::ios);
};

ip::address remote_address(const sock &);
std::string remote_ip(const sock &);
uint16_t remote_port(const sock &);
ip::address local_address(const sock &);
std::string local_ip(const sock &);
uint16_t local_port(const sock &);

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


static_assert(BUFSIZE == 512, "");

}      // namespace ircd
#endif // __cplusplus
