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
#define HAVE_IRCD_M_TYPING_H

namespace ircd::m::typing
{
	struct typist;
	struct commit;

	using edu = m::edu::m_typing;
	using closure = std::function<bool (const edu &)>;

	// Iterate all of the active typists held in RA<
	//NOTE: no yielding in this iteration.
	bool for_each(const closure &);

	// Get whether a user enabled typing events for a room. The type string
	// can be "send" or "sync" prevent typing one's events from being sent or
	// others' from being sync'ed, respectively
	bool allow(const id::user &, const id::room &, const string_view &type);
}

/// Interface to update the typing state, generate all events, send etc.
struct ircd::m::typing::commit
{
	commit(const edu &);
};

struct ircd::m::edu::m_typing
:json::tuple
<
	json::property<name::user_id, json::string>,
	json::property<name::room_id, json::string>,
	json::property<name::timeout, time_t>,
	json::property<name::typing, bool>
>
{
	using super_type::tuple;
	using super_type::operator=;
};

struct ircd::m::typing::typist
{
	using is_transparent = void;

	system_point timesout;
	m::user::id::buf user_id;
	m::room::id::buf room_id;

	bool operator()(const typist &a, const string_view &b) const noexcept;
	bool operator()(const string_view &a, const typist &b) const noexcept;
	bool operator()(const typist &a, const typist &b) const noexcept;
};
