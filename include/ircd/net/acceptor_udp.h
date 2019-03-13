// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_NET_ACCEPTOR_UDP_H

// This file is not included with the IRCd standard include stack because
// it requires symbols we can't forward declare without boost headers. It
// is part of the <ircd/asio.h> stack which can be included in your
// definition file if you need low level access to this acceptor API.

struct ircd::net::acceptor_udp
{
	using error_code = boost::system::error_code;
	using datagram = listener_udp::datagram;
	using flag = listener_udp::flag;

	IRCD_EXCEPTION(listener_udp::error, error)

	static constexpr log::log &log {acceptor::log};
	static ip::udp::socket::message_flags flags(const flag &);

	std::string name;
	std::string opts;
	ip::udp::endpoint ep;
	ip::udp::socket a;
	size_t waiting {0};
	ctx::dock joining;

	// Yield context for datagram.
	datagram &operator()(datagram &);

	// Acceptor shutdown
	bool interrupt() noexcept;
	void join() noexcept;

	acceptor_udp(const string_view &name,
	             const json::object &opts);

	~acceptor_udp() noexcept;
};
