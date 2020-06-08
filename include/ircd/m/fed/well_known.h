// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_FED_WELL_KNOWN_H

namespace ircd::m::fed::well_known
{
	string_view fetch(const mutable_buffer &out, const string_view &origin);
	string_view get(const mutable_buffer &out, const string_view &origin);

	extern conf::item<size_t> fetch_redirects;
	extern conf::item<seconds> fetch_timeout;
	extern conf::item<seconds> cache_max;
	extern conf::item<seconds> cache_error;
	extern conf::item<seconds> cache_default;
}
