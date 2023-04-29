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
:returns<bool>
{
	struct opts;

  private:
	static const event::keys::exclude exclude_keys;
	static const event::keys default_keys;
	static conf::item<std::string> exclude_types;
	static conf::item<bool> info;
	static log::log log;

	bool is_ignored(const event &, const opts &) const;
	bool is_redacted(const event &, const opts &) const;
	bool is_invisible(const event &, const opts &) const;
	bool is_excluded(const event &, const opts &) const;

	void _age(json::stack::object &, const event &, const opts &);
	void _txnid(json::stack::object &, const event &, const opts &);
	void _prev_state(json::stack::object &, const event &, const opts &);
	void _unsigned(json::stack::object &, const event &, const opts &);

	bool members(json::stack::object &, const event &, const opts &);
	bool object(json::stack::array &, const event &, const opts &);

  public:
	append(json::stack::object &, const event &, const opts &);
	append(json::stack::object &, const event &);
	append(json::stack::array &, const event &, const opts &);
	append(json::stack::array &, const event &);
};

/// Provide as much information as you can apropos this event so the impl
/// can provide the best result.
struct ircd::m::event::append::opts
{
	event::idx event_idx {0};
	string_view client_txnid;
	id::user user_id;
	id::room user_room_id;
	int64_t room_depth {-1L};
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

inline
ircd::m::event::append::append(json::stack::array &array,
                               const event &event,
                               const opts &opts)
:returns<bool>{[this, &array, &event, &opts]
{
	return object(array, event, opts);
}}
{}

inline
ircd::m::event::append::append(json::stack::object &object,
                               const event &event,
                               const opts &opts)
:returns<bool>{[this, &object, &event, &opts]
{
	return members(object, event, opts);
}}
{}
