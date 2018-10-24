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
	// All rooms known to IRCd
	void for_each(const room::id::closure_bool &);
	void for_each(const room::id::closure &);
	void for_each(const room::closure_bool &);
	void for_each(const room::closure &);

	// All rooms for a user (alias to interface in user::)
	void for_each(const user &, const string_view &membership, const user::rooms::closure_bool &);
	void for_each(const user &, const string_view &membership, const user::rooms::closure &);
	void for_each(const user &, const user::rooms::closure_bool &);
	void for_each(const user &, const user::rooms::closure &);

	// Linkage to utils that build a publicrooms summary from room state.
	void summary_chunk(const m::room &, json::stack::object &chunk);
	json::object summary_chunk(const m::room &, const mutable_buffer &out);
}
