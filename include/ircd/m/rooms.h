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
#define HAVE_IRCD_M_ROOMS_H

/// Convenience iterface for rooms; utilities for the server's collection of
/// rooms and some rooms list related linkages.
///
namespace ircd::m::rooms
{
	struct each_opts;

	bool is_public(const room::id &);
	size_t count_public(const string_view &server = {});

	bool for_each(const each_opts &);
	template<class... args> bool for_each(args&&...);

	// Linkage to utils that build a publicrooms summary from room state.
	void summary_chunk(const m::room &, json::stack::object &chunk);
	json::object summary_chunk(const m::room &, const mutable_buffer &out);
	event::id::buf summary_set(const m::room::id &, const json::object &summary);
	event::id::buf summary_set(const m::room &);
	event::id::buf summary_del(const m::room &);
}

/// Arguments structure to rooms::for_each(). This reduces the API surface to
/// handle a rich set of ways to iterate over the rooms. Several convenience
/// constructors based on common usage are provided; note that these are not
/// the only options patterns.
struct ircd::m::rooms::each_opts
{
	/// If set: select rooms for this user.
	m::user user;

	/// If user is set, and this is set: select the membership that user
	/// has in the room.
	string_view membership;

	/// If set, seek to this room_id or lower-bound to the closest room_id.
	string_view key;

	/// All public rooms only; key may be a server hostname or room_id.
	/// - for server hostname: iterates known rooms from that server.
	/// - for room_id: starts iteration from that (or closest) room_id
	///   which can be used as a since/pagination token.
	bool public_rooms {false};

	/// Principal closure for results; invoked synchronously during the call;
	/// for-each protocol: return true to continue, false to break.
	room::id::closure_bool closure;

	/// Alternative closure when selecting a user.
	user::rooms::closure_bool user_closure;

	each_opts(room::id::closure_bool);
	each_opts(const string_view &key, room::id::closure_bool);
	each_opts(const m::user &user, user::rooms::closure_bool);
	each_opts(const m::user &user, const string_view &membership, user::rooms::closure_bool);
	each_opts() = default;
};

inline
ircd::m::rooms::each_opts::each_opts(room::id::closure_bool closure)
:closure{std::move(closure)}
{}

inline
ircd::m::rooms::each_opts::each_opts(const string_view &key,
                                     room::id::closure_bool closure)
:key{key}
,closure{std::move(closure)}
{}

inline
ircd::m::rooms::each_opts::each_opts(const m::user &user,
                                     user::rooms::closure_bool user_closure)
:user{user}
,user_closure{std::move(user_closure)}
{}

inline
ircd::m::rooms::each_opts::each_opts(const m::user &user,
	                                 const string_view &membership,
	                                 user::rooms::closure_bool user_closure)
:user{user}
,membership{membership}
,user_closure{std::move(user_closure)}
{}

template<class... args>
bool
ircd::m::rooms::for_each(args&&... a)
{
	const each_opts opts
	{
		std::forward<args>(a)...
	};

	return for_each(opts);
}
