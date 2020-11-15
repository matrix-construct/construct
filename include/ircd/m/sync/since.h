// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_SYNC_SINCE_H

namespace ircd::m::sync
{
	using since = std::tuple<event::idx, event::idx, string_view>;

	event::idx sequence(const since &);
	since make_since(const string_view &);
	string_view make_since(const mutable_buffer &, const m::events::range &, const string_view &flags = {});
	string_view make_since(const mutable_buffer &, const int64_t &, const string_view &flags = {});
}

inline ircd::m::event::idx
ircd::m::sync::sequence(const since &since)
{
	const auto &[token, snapshot, flags]
	{
		since
	};

	return snapshot?: token?: -1UL;
}
