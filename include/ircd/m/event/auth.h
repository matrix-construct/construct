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
#define HAVE_IRCD_M_EVENT_AUTH_H

namespace ircd::m
{
	bool for_each(const event::auth &, const event::id::closure_bool &);

	std::ostream &pretty(std::ostream &, const event::auth &);
	std::string pretty(const event::auth &);

	std::ostream &pretty_oneline(std::ostream &, const event::auth &);
	std::string pretty_oneline(const event::auth &);
}

/// Interface to the auth-references of an event. This interface overlays
/// on the m::event tuple and adds functionality focused specifically on the
/// various reference properties in the event data.
///
struct ircd::m::event::auth
:json::tuple
<
	json::property<name::auth_events, json::array>
>
{
	static constexpr const size_t &MAX
	{
		5
	};

	template<size_t N> vector_view<event::id> ids(event::id (&)[N]) const;
	template<size_t N> vector_view<event::idx> idxs(event::idx (&)[N]) const;
	std::tuple<event::id, json::object> auth_events(const size_t &idx) const;
	event::id auth_event(const size_t &idx) const;
	bool auth_event_exists(const size_t &idx) const;
	bool auth_events_has(const event::id &) const;
	size_t auth_events_count() const;
	size_t auth_events_exist() const;
	bool auth_exist() const;

	using super_type::tuple;
	using super_type::operator=;
};

template<size_t N>
inline ircd::vector_view<ircd::m::event::idx>
ircd::m::event::auth::idxs(event::idx (&out)[N])
const
{
	return vector_view<event::idx>
	(
		out, m::index(out, *this)
	);
}

template<size_t N>
inline ircd::vector_view<ircd::m::event::id>
ircd::m::event::auth::ids(event::id (&out)[N])
const
{
	size_t i(0);
	m::for_each(*this, [&i, &out](const event::id &event_id)
	{
		out[i++] = event_id;
		return i < N;
	});

	return vector_view<event::id>
	(
		out, i
	);
}
