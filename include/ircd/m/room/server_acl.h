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
#define HAVE_IRCD_M_ROOM_SERVER_ACL_H

/// Interface to the server access control lists
///
/// This interface focuses specifically on the state event type
/// `m.room.server_acl` which allows for access control at server scope. This
/// is necessary because access controls via `m.room.member` operate at the
/// scope of individual `state_key` cells in the room state, thus lacking the
/// ability to assert control over multiple cells and those which do not yet
/// exist.
///
/// Primary use of this interface is with operator() which returns true if
/// the server is permitted by the room's ACL and false if denied. This is
/// determined by the ACL event content only. Exceptions are not intended to be
/// thrown. The conf items resident in this class do not actually affect the
/// results of the member functions; they are for users to determine how/if
/// to invoke this interface.
///
struct ircd::m::room::server_acl
{
	using closure_bool = std::function<bool (const string_view &)>;
	using view_closure = std::function<void (const json::object &)>;

	static conf::item<bool> enable_write;  // request origin / event origin
	static conf::item<bool> enable_read;   // request origin
	static conf::item<bool> enable_fetch;  // request destination / event origin
	static conf::item<bool> enable_send;   // request destination

	m::room room;
	event::idx event_idx {0};
	mutable json::object content;

	bool view(const view_closure &) const;
	bool check(const net::hostport &server) const;

  public:
	bool exists() const;

	// Iterate a list property; spec properties are "allow" and "deny".
	bool for_each(const string_view &prop, const closure_bool &) const;
	size_t count(const string_view &prop) const;
	bool has(const string_view &prop) const;

	// Get top-level boolean value returning 1 or 0; -1 for not found.
	int getbool(const string_view &prop) const;

	// Test if *exact string* is listed in property list; not expr match.
	bool has(const string_view &prop, const string_view &expr) const;

	// Test if host is expression-matched in property list.
	bool match(const string_view &prop, const net::hostport &server) const;

	// Test if server passes or fails the ACL; this factors matching in
	// "allow", "deny" and "allow_ip_literals" per the input with any default.
	bool operator()(const net::hostport &server) const;

	server_acl(const m::room &, const event::idx &acl_event_idx);
	server_acl(const m::room &, const json::object &content);
	server_acl(const m::room &);
	server_acl() = default;

	static bool check(const m::id::room &, const net::hostport &server) noexcept;
};

inline
ircd::m::room::server_acl::server_acl(const m::room &room)
:server_acl{room, event::idx{0}}
{}

inline
ircd::m::room::server_acl::server_acl(const m::room &room,
                                      const json::object &content)
:room{room}
,content{content}
{}
