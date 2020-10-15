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
	struct opts;

	ctx::future<string_view>
	get(const mutable_buffer &out, const string_view &name, const opts &);

	extern conf::item<size_t> fetch_redirects;
	extern conf::item<seconds> fetch_timeout;
	extern conf::item<seconds> cache_max;
	extern conf::item<seconds> cache_error;
	extern conf::item<seconds> cache_default;
}

struct ircd::m::fed::well_known::opts
{
	/// Whether to check the cache before making any request.
	bool cache_check {true};

	/// Allow expired cache results to be returned without making any refresh.
	bool expired {false};

	/// Allow a query to be made to a remote.
	bool request {true};

	/// Whether to cache the result of any request.
	bool cache_result {true};
};
