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
#define HAVE_IRCD_M_ROOM_MEMBERS_H

/// Interface to the aliases
///
/// This interface focuses specifically on room aliases. These are aliases contained
/// in a room's state.
///
struct ircd::m::room::aliases
{
	using closure_bool = std::function<bool (const alias &)>;

	static bool for_each(const m::room &, const string_view &server, const closure_bool &);

	m::room room;

  public:
	bool for_each(const string_view &server, const closure_bool &) const;
	bool for_each(const closure_bool &) const;
	bool has(const alias &) const;
	size_t count(const string_view &server) const;
	size_t count() const;

	aliases(const m::room &room)
	:room{room}
	{}
};
