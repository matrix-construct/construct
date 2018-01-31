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
	struct tag;
	using header = rfc1035::header;

	std::vector<ip::udp::endpoint> server;       // The list of active servers
	size_t server_next{0};                       // Round-robin state to hit servers
	void init_servers();

	ctx::dock dock;
	std::map<uint16_t, tag> tags;                // The active requests

	ip::udp::socket ns;                          // A pollable activity object
	ip::udp::endpoint reply_from;
	char reply[64_KiB] alignas(16);

	bool handle_error(const error_code &ec) const;
	void handle_reply(const header &, const const_buffer &body, tag &);
	void handle_reply(const header &, const const_buffer &body);
	void handle(const error_code &ec, const size_t &) noexcept;
	void set_handle();

	void send_query(const ip::udp::endpoint &, const const_buffer &);
	void send_query(const const_buffer &);

	void operator()(const hostport &, const flag &, callback_many);
	void operator()(const ipport &, const flag &, callback_reverse);

	bool check_timeout(const uint16_t &id, tag &, const steady_point &now);
	void check_timeouts();
	void worker();
	ctx::context context;

	resolver();
	~resolver() noexcept;
};

struct ircd::net::dns::resolver::tag
{
	hostport hp;
	ipport ipp;
	flag flags;
	callback_many cb_many;
	callback_reverse cb_reverse;
	steady_point last {ircd::now<steady_point>()};
	uint8_t tries {0};

	void set_exception(std::exception_ptr &&);

	tag(const hostport &hp, const flag &flags, callback_many cb_many)
	:hp{hp}
	,flags{flags}
	,cb_many{std::move(cb_many)}
	{}

	tag(const ipport &ipp, const flag &flags, callback_reverse cb_reverse)
	:ipp{ipp}
	,flags{flags}
	,cb_reverse{std::move(cb_reverse)}
	{}
};
