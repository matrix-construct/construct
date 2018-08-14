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
#define HAVE_IRCD_M_SELF_H

namespace ircd::m::self
{
	struct init;

	string_view host();
	bool host(const string_view &);
}

namespace ircd::m
{
	extern struct user me;
	extern struct room my_room;
	extern struct node my_node;

	string_view my_host();
	bool my_host(const string_view &);
}

namespace ircd
{
	using m::my_host;
}

struct ircd::m::self::init
{
	init(const string_view &origin);
};

inline ircd::string_view
ircd::m::my_host()
{
	return self::host();
}

inline bool
ircd::m::my_host(const string_view &host)
{
	return self::host(host);
}
