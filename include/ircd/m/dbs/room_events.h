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
#define HAVE_IRCD_M_DBS_ROOM_EVENTS_H

namespace ircd::m::dbs
{
	constexpr size_t ROOM_EVENTS_KEY_MAX_SIZE
	{
		id::MAX_SIZE + 1 + 8 + 8
	};

	string_view room_events_key(const mutable_buffer &out, const id::room &, const uint64_t &depth, const event::idx &);
	string_view room_events_key(const mutable_buffer &out, const id::room &, const uint64_t &depth);
	std::tuple<uint64_t, event::idx> room_events_key(const string_view &amalgam);

	void _index_room_events(db::txn &, const event &, const write_opts &);

	// room_id | depth, event_idx => node_id
	extern db::domain room_events;
}

namespace ircd::m::dbs::desc
{
	// room events sequence
	extern conf::item<std::string> room_events__comp;
	extern conf::item<size_t> room_events__block__size;
	extern conf::item<size_t> room_events__meta_block__size;
	extern conf::item<size_t> room_events__cache__size;
	extern conf::item<size_t> room_events__cache_comp__size;
	extern const db::prefix_transform room_events__pfx;
	extern const db::comparator room_events__cmp;
	extern const db::descriptor room_events;
}
