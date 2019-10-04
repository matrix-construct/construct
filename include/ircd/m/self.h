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
	// Test if provided string is one of my homeserver's network_name()'s.
	bool my_host(const string_view &);

	// Similar to my_host(), but the comparison is relaxed to allow port
	// numbers to be a factor: myself.com:8448 input will match if a homeserver
	// here has a network_name of myself.com.
	bool host(const string_view &);

	// Alias for origin(my()); primary homeserver's network name
	string_view my_host(); //__attribute__((deprecated));
}

namespace ircd::m
{
	using self::my_host;
	using self::host;
}

namespace ircd
{
	using m::my_host;
}
