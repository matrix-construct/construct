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
#define HAVE_IRCD_M_ROOM_HEAD_GENERATE_H

struct ircd::m::room::head::generate
{
	struct opts;

	/// won't be set when using json::stack overload
	json::array array;

	/// lowest and highest depths in results
	std::array<int64_t, 2> depth
	{
		std::numeric_limits<int64_t>::max(),
		std::numeric_limits<int64_t>::min(),
	};

	generate(json::stack::array &, const m::room::head &, const opts &);
	generate(const mutable_buffer &, const m::room::head &, const opts &);
	generate() = default;
};

struct ircd::m::room::head::generate::opts
{
	/// Limit the number of result elements.
	size_t limit {16};

	/// Requires that at least one reference is at the highest known depth.
	bool need_top_head {false};

	/// Requires that at least one reference is to an event created by this
	/// server (origin).
	bool need_my_head {false};

	/// Hint the room version which determines the output format; avoid query.
	string_view version;
};
