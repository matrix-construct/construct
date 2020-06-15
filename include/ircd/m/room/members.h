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
#define HAVE_IRCD_M_ROOM_MEMBERS_H

/// Interface to the members of a room.
///
/// This interface focuses specifically on room membership and its routines
/// are optimized for this area of room functionality.
///
struct ircd::m::room::members
{
	using closure_idx = std::function<bool (const id::user &, const event::idx &)>;
	using closure = std::function<bool (const id::user &)>;

	m::room room;

	bool for_each_join_present(const string_view &host, const closure_idx &) const;

  public:
	bool for_each(const string_view &membership, const string_view &host, const closure &) const;
	bool for_each(const string_view &membership, const string_view &host, const closure_idx &) const;
	bool for_each(const string_view &membership, const closure &) const;
	bool for_each(const string_view &membership, const closure_idx &) const;
	bool for_each(const closure &) const;
	bool for_each(const closure_idx &) const;

	bool empty(const string_view &membership, const string_view &host) const;
	bool empty(const string_view &membership) const;
	bool empty() const;

	size_t count(const string_view &membership, const string_view &host) const;
	size_t count(const string_view &membership) const;
	size_t count() const;

	members(const m::room &room)
	:room{room}
	{}
};
