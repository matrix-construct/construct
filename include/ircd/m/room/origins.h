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
#define HAVE_IRCD_M_ROOM_ORIGINS_H

/// Interface to the origins (autonomous systems) of a room
///
/// This interface focuses specifically on the origins (from the field in the
/// event object) which are servers/networks/autonomous systems, or something.
/// Messages have to be sent to them, and an efficient iteration of the
/// origins as provided by this interface helps with that.
///
struct ircd::m::room::origins
{
	using closure = std::function<void (const string_view &)>;
	using closure_bool = std::function<bool (const string_view &)>;

	m::room room;

	bool _for_each_(const closure_bool &view) const;
	bool for_each(const closure_bool &view) const;
	void for_each(const closure &view) const;
	bool has(const string_view &origin) const;
	bool only(const string_view &origin) const;
	size_t count() const;

	// select an origin in the room at random; use proffer to refuse and try another.
	string_view random(const mutable_buffer &buf, const closure_bool &proffer = nullptr) const;
	bool random(const closure &, const closure_bool &proffer = nullptr) const;

	origins(const m::room &room)
	:room{room}
	{}
};
