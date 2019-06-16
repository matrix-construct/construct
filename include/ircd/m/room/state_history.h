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
#define HAVE_IRCD_M_ROOM_STATE_HISTORY_H

/// Interface to the state of a room at some previous point in time. This is
/// constructed out of the data obtained through the lower-level state::space
/// interface.
///
struct ircd::m::room::state::history
{
	using closure = std::function<bool (const string_view &, const string_view &, const int64_t &, const event::idx &)>;

	state::space space;
	int64_t bound {-1};

  public:
	bool for_each(const string_view &type, const string_view &state_key, const closure &) const;
	bool for_each(const string_view &type, const closure &) const;
	bool for_each(const closure &) const;

	size_t count(const string_view &type, const string_view &state_key) const;
	size_t count(const string_view &type) const;

	bool has(const string_view &type, const string_view &state_key) const;
	bool has(const string_view &type) const;

	event::idx get(std::nothrow_t, const string_view &type, const string_view &state_key) const;
	event::idx get(const string_view &type, const string_view &state_key) const;

	history(const m::room &, const int64_t &bound);
	history(const m::room::id &, const m::event::id &);
	history(const m::room &);
};
