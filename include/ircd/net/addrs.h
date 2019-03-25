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
#define HAVE_IRCD_NET_ADDRS_H

extern "C"
{
	struct ifaddrs;
}

namespace ircd::net::addrs
{
	struct addr;
	using closure = std::function<bool (const addr &)>;
	using raw_closure = std::function<bool (const struct ::ifaddrs &)>;

	bool for_each(const raw_closure &);
	bool for_each(const closure &);
}

struct ircd::net::addrs::addr
{
	string_view name;
	ipport address;
	uint32_t flags {0};
	uint32_t flowinfo {0};
	uint32_t scope_id {0};
	uint16_t family {0};
};
