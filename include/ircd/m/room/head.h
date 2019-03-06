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
#define HAVE_IRCD_M_ROOM_HEAD_H

namespace ircd::m
{
	// [GET] Current Event ID and depth suite (non-locking) (one only)
	std::tuple<id::event::buf, int64_t, event::idx> top(std::nothrow_t, const id::room &);
	std::tuple<id::event::buf, int64_t, event::idx> top(const id::room &);

	// [GET] Current Event ID (non-locking) (one only)
	id::event::buf head(std::nothrow_t, const id::room &);
	id::event::buf head(const id::room &);

	// [GET] Current Event idx (non-locking) (one only)
	event::idx head_idx(std::nothrow_t, const id::room &);
	event::idx head_idx(const id::room &);

	// [GET] Current Event depth (non-locking)
	int64_t depth(std::nothrow_t, const id::room &);
	int64_t depth(const id::room &);
}

/// Interface to the room head
///
/// This interface helps compute and represent aspects of the room graph,
/// specifically concerning the "head" or the "front" or the "top" of this
/// graph where events are either furthest from the m.room.create genesis,
/// or are yet unreferenced by another event. Usage of this interface is
/// fundamental when composing the references of a new event on the graph.
///
struct ircd::m::room::head
{
	using closure = std::function<void (const event::idx &, const event::id &)>;
	using closure_bool = std::function<bool (const event::idx &, const event::id &)>;

	static size_t reset(const head &);
	static size_t rebuild(const head &);
	static void modify(const event::id &, const db::op &, const bool &);
	static int64_t make_refs(const head &, json::stack::array &, const size_t &, const bool &);
	static bool for_each(const head &, const closure_bool &);

  public:
	m::room room;

	bool for_each(const closure_bool &) const;
	void for_each(const closure &) const;
	bool has(const event::id &) const;
	size_t count() const;

	int64_t make_refs(json::stack::array &, const size_t &, const bool &) const;
	std::pair<json::array, int64_t> make_refs(const mutable_buffer &, const size_t &, const bool &) const;

	head(const m::room &room)
	:room{room}
	{}
};
