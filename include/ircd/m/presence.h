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
#define HAVE_IRCD_M_PRESENCE_H

namespace ircd::m
{
	struct presence;
}

struct ircd::m::edu::m_presence
:json::tuple
<
	json::property<name::user_id, json::string>,
	json::property<name::presence, json::string>,
	json::property<name::status_msg, json::string>,
	json::property<name::last_active_ago, time_t>,
	json::property<name::currently_active, bool>
>
{
	using super_type::tuple;
	using super_type::operator=;
};

struct ircd::m::presence
:edu::m_presence
{
	using closure = std::function<void (const json::object &)>;
	using closure_event = std::function<void (const m::event &)>;

	static bool valid_state(const string_view &state);

	static event::idx get(std::nothrow_t, const user &);
	static event::idx get(const user &);

	static bool get(std::nothrow_t, const user &, const closure_event &, const event::fetch::opts *const & = nullptr);
	static bool get(std::nothrow_t, const user &, const closure &);
	static void get(const user &, const closure &);

	static event::id::buf set(const presence &);
	static event::id::buf set(const user &, const string_view &, const string_view &status = {});

	using edu::m_presence::m_presence;
	presence(const user &, const mutable_buffer &);
};
