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
	namespace ip = asio::ip;
	using asio::steady_timer;
	using ircd::string;

	uint16_t port(const ip::tcp::endpoint &);
	ip::address addr(const ip::tcp::endpoint &);
	std::string host(const ip::tcp::endpoint &);
	std::string string(const ip::address &);
	std::string string(const ip::tcp::endpoint &);

	ipport make_ipport(const boost::asio::ip::tcp::endpoint &);
	ipport make_ipport(const boost::asio::ip::udp::endpoint &);
	ip::tcp::endpoint make_endpoint(const ipport &);
	ip::udp::endpoint make_endpoint_udp(const ipport &);
}

#include <ircd/net/socket.h>
#include <ircd/net/acceptor.h>

namespace ircd
{
	using net::string;
	using net::addr;
	using net::host;
	using net::port;
}
