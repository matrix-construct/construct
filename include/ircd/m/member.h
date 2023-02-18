// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_MEMBER_H

namespace ircd::m
{
	bool member(const event &, const string_view &membership);
	bool member(const event &, const vector_view<const string_view> &);
}

/// Events that are not type=m.room.member will return false; the rest will be
/// passed to the analogous m::membership function which does not check type.
inline bool
ircd::m::member(const event &event,
                const vector_view<const string_view> &membership)
{
	if(json::get<"type"_>(event) != "m.room.member")
		return false;

	return m::membership(event, membership);
}

/// Events that are not type=m.room.member will return false; the rest will be
/// passed to the analogous m::membership function which does not check type.
inline bool
ircd::m::member(const event &event,
                const string_view &membership)
{
	if(json::get<"type"_>(event) != "m.room.member")
		return false;

	return m::membership(event) == membership;
}
