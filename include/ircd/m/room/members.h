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
	using closure = std::function<void (const id::user &)>;
	using closure_bool = std::function<bool (const id::user &)>;

	m::room room;

	bool for_each(const string_view &membership, const event::closure_bool &) const;
	void for_each(const string_view &membership, const event::closure &) const;
	bool for_each(const string_view &membership, const closure_bool &) const;
	void for_each(const string_view &membership, const closure &) const;
	bool for_each(const event::closure_bool &) const;
	void for_each(const event::closure &) const;
	bool for_each(const closure_bool &) const;
	void for_each(const closure &) const;

	size_t count(const string_view &membership) const;
	size_t count() const;

	members(const m::room &room)
	:room{room}
	{}
};
