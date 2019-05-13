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
#define HAVE_IRCD_M_ROOM_STATE_SPACE_H

/// Interface to all room states.
///
struct ircd::m::room::state::space
{
	struct rebuild;
	using closure = std::function<bool (const string_view &, const string_view &, const int64_t &, const event::idx &)>;

	m::room room;

	bool for_each(const string_view &type, const string_view &state_key, const int64_t &depth, const closure &) const;
	bool for_each(const string_view &type, const string_view &state_key, const closure &) const;
	bool for_each(const string_view &type, const closure &) const;
	bool for_each(const closure &) const;

	size_t count(const string_view &type, const string_view &state_key, const int64_t &depth) const;
	size_t count(const string_view &type, const string_view &state_key) const;
	size_t count(const string_view &type) const;

	bool has(const string_view &type, const string_view &state_key, const int64_t &depth) const;
	bool has(const string_view &type, const string_view &state_key) const;
	bool has(const string_view &type) const;

	space(const m::room &);
};

struct ircd::m::room::state::space::rebuild
{
	rebuild();
};
