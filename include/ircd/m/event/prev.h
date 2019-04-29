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
#define HAVE_IRCD_M_EVENT_PREV_H

namespace ircd::m
{
	bool for_each(const event::prev &, const event::id::closure_bool &);
	void for_each(const event::prev &, const event::id::closure &);
	size_t degree(const event::prev &);
	size_t count(const event::prev &);

	std::ostream &pretty(std::ostream &, const event::prev &);
	std::string pretty(const event::prev &);

	std::ostream &pretty_oneline(std::ostream &, const event::prev &);
	std::string pretty_oneline(const event::prev &);
}

struct ircd::m::event::prev
:json::tuple
<
	json::property<name::auth_events, json::array>,
	json::property<name::prev_state, json::array>,
	json::property<name::prev_events, json::array>
>
{
	enum cond :int;

	std::tuple<event::id, string_view> auth_events(const size_t &idx) const;
	std::tuple<event::id, string_view> prev_states(const size_t &idx) const;
	std::tuple<event::id, string_view> prev_events(const size_t &idx) const;

	event::id auth_event(const size_t &idx) const;
	event::id prev_state(const size_t &idx) const;
	event::id prev_event(const size_t &idx) const;

	size_t auth_events_count() const;
	size_t prev_states_count() const;
	size_t prev_events_count() const;

	bool auth_events_has(const event::id &) const;
	bool prev_states_has(const event::id &) const;
	bool prev_events_has(const event::id &) const;

	using super_type::tuple;
	using super_type::operator=;
};
