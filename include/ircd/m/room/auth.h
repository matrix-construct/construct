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
#define HAVE_IRCD_M_ROOM_AUTH_H

/// Interface to the auth_chain / auth_dag.
///
struct ircd::m::room::auth
{
	using closure_bool = std::function<bool (const event::idx &)>;
	using closure = std::function<void (const event::idx &)>;
	using types = vector_view<const string_view>;

	static bool for_each(const auth &, const closure_bool &);
	static void make_refs(const auth &, json::stack::array &, const types &, const m::id::user & = {});

	m::room room;

  public:
	bool for_each(const closure_bool &) const;
	void for_each(const closure &) const;

	void make_refs(json::stack::array &, const types &, const m::id::user & = {}) const;
	json::array make_refs(const mutable_buffer &, const types &, const m::id::user & = {}) const;

	auth(const m::room &room)
	:room{room}
	{}
};
