// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_FED_EVENT_NEAR_H

namespace ircd::m::fed
{
	struct event_near;
}

struct ircd::m::fed::event_near
:request
{
	explicit operator json::object() const;

	/// if ts > 0, dir=f;
	/// if <= 0, dir=b;
	/// if ts == 0, ts = now, dir = b;
	event_near(const m::room::id &,
	           const mutable_buffer &,
	           const int64_t ts,
	           opts);

	event_near() = default;
};

inline
ircd::m::fed::event_near::operator
ircd::json::object()
const
{
	const json::object object
	{
		in.content
	};

	return object;
}
