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
#define HAVE_IRCD_M_USER_NOTIFICATIONS_H

struct ircd::m::user::notifications
{
	struct opts;
	using closure_meta = std::function<bool (const string_view &type, const event::idx &)>;
	using closure = std::function<bool (const event::idx &, const json::object &)>;

	static const string_view type_prefix;

	static string_view make_type(const mutable_buffer &, const opts &);
	static opts unmake_type(const string_view &type);

	m::user user;

  public:
	bool for_each(const opts &, const closure_meta &) const;
	bool for_each(const opts &, const closure &) const;
	size_t count(const opts &) const;
	bool empty(const opts &) const;

	notifications(const m::user &user) noexcept;
};

struct ircd::m::user::notifications::opts
{
	event::idx from {0};     // highest idx counting down
	event::idx to {0};       // lowest idx ending iteration
	string_view only;        // spec "only" filter
	room::id room_id;        // room_id filter (optional)
	bool sorted {true};      // sorted or optimal results
};

inline
ircd::m::user::notifications::notifications(const m::user &user)
noexcept
:user{user}
{}
