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
#define HAVE_IRCD_NET_ASIO_H

// This file is not included with the IRCd standard include stack because
// it requires symbols we can't forward declare without boost headers. It
// is part of the <ircd/asio.h> stack which can be included in your
// definition file if you need low level access to this socket API. The
// <ircd/net/net.h> still offers higher level access to sockets without
// requiring boost headers; please check that for satisfaction before
// including this.

namespace ircd::net
{
	using boost::system::error_code;
	string_view string(const mutable_buffer &, const boost::system::error_code &);
	string_view string(const mutable_buffer &, const boost::system::system_error &);
	std::string string(const boost::system::error_code &);
	std::string string(const boost::system::system_error &);

	namespace ip = asio::ip;
	uint16_t port(const ip::tcp::endpoint &);
	ip::address addr(const ip::tcp::endpoint &);
	std::string host(const ip::tcp::endpoint &);
	std::string string(const ip::address &);
	std::string string(const ip::tcp::endpoint &);

	ipport make_ipport(const boost::asio::ip::tcp::endpoint &);
	ip::tcp::endpoint make_endpoint(const ipport &);
}

namespace ircd
{
	using net::error_code;
	using net::string;
	using net::addr;
	using net::host;
	using net::port;
}
