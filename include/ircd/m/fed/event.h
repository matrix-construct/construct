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
#define HAVE_IRCD_M_FED_EVENT_H

namespace ircd::m::fed
{
	struct event;
};

struct ircd::m::fed::event
:request
{
	explicit operator json::object() const;
	explicit operator m::event() const;

	event(const m::event::id &,
	      const mutable_buffer &,
	      opts);

	event() = default;
};

inline
ircd::m::fed::event::operator
ircd::m::event()
const
{
	return json::object{*this};
}

inline
ircd::m::fed::event::operator
ircd::json::object()
const
{
	const json::object object
	{
		in.content
	};

	const json::array pdus
	{
		object.at("pdus")
	};

	return pdus.at(0);
}
