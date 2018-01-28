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
#define HAVE_IRCD_NET_RESOLVER_H

// This file is not included with the IRCd standard include stack because
// it requires symbols we can't forward declare without boost headers. It
// is part of the <ircd/asio.h> stack which can be included in your
// definition file if you need low level access to this resolver API.

/// Internal resolver service
struct ircd::net::dns::resolver
{
	using resolve_callback = std::function<void (std::exception_ptr, ip::tcp::resolver::results_type)>;

	ip::tcp::resolver gai;                       // Old getaddrinfo() being removed
	ip::udp::socket ns;                          // A pollable activity object
	std::vector<ip::udp::endpoint> server;       // The list of active servers
	size_t server_next{0};                       // Round-robin state to hit servers

	bool reply_set {false};
	ip::udp::endpoint reply_from;
	uint8_t reply[64_KiB];

	void handle(const error_code &ec, const size_t &) noexcept;
	void set_handle();

	void send_query(const ip::udp::endpoint &, const const_buffer &);
	void send_query(const const_buffer &);

	void operator()(const hostport &, ip::tcp::resolver::flags, resolve_callback);
	void operator()(const ipport &, resolve_callback);

	resolver();
	~resolver() noexcept;
};
