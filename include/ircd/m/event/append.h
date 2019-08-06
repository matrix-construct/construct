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
#define HAVE_IRCD_M_EVENT_APPEND

struct ircd::m::event::append
:boolean
{
	struct opts;

	append(json::stack::object &, const event &, const opts &);
	append(json::stack::object &, const event &);
	append(json::stack::array &, const event &, const opts &);
	append(json::stack::array &, const event &);
};

struct ircd::m::event::append::opts
{
	const event::idx *event_idx {nullptr};
	const string_view *client_txnid {nullptr};
	const id::user *user_id {nullptr};
	const room *user_room {nullptr};
	const int64_t *room_depth {nullptr};
	const event::keys *keys {nullptr};
	long age {std::numeric_limits<long>::min()};
	bool query_txnid {true};
};

inline
ircd::m::event::append::append(json::stack::array &a,
                               const event &e)
:append{a, e, {}}
{}

inline
ircd::m::event::append::append(json::stack::object &o,
                               const event &e)
:append{o, e, {}}
{}
