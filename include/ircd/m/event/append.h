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

//XXX fwd decl
namespace ircd::m
{
	struct room;
	struct event_filter;
};

/// Used when transmitting events to clients. This tries to hide and provide
/// as much boilerplate as possible which we abstracted from all of the
/// different locations where an event may be revealed to a client. This device
/// will add things like a client txnid, calculate and add an `unsigned.age`,
/// find and add the prev_state/prev_content for state events, etc.
///
struct ircd::m::event::append
:boolean
{
	struct opts;

	append(json::stack::object &, const event &, const opts &);
	append(json::stack::object &, const event &);
	append(json::stack::array &, const event &, const opts &);
	append(json::stack::array &, const event &);
};

/// Provide as much information as you can apropos this event so the impl
/// can provide the best result.
struct ircd::m::event::append::opts
{
	const event::idx *event_idx {nullptr};
	const string_view *client_txnid {nullptr};
	const id::user *user_id {nullptr};
	const room *user_room {nullptr};
	const int64_t *room_depth {nullptr};
	const event::keys *keys {nullptr};
	const m::event_filter *event_filter {nullptr};
	long age {std::numeric_limits<long>::min()};
	bool query_txnid {true};
	bool query_prev_state {true};
	bool query_redacted {true};
	bool query_visible {false};
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
