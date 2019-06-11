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
	std::pair<uint64_t, event::idx> room_events_key(const string_view &amalgam);

	// room_id | depth, event_idx => node_id
	extern db::domain room_events;
}

namespace ircd::m::dbs::desc
{
	// room events sequence
	extern conf::item<size_t> events__room_events__block__size;
	extern conf::item<size_t> events__room_events__meta_block__size;
	extern conf::item<size_t> events__room_events__cache__size;
	extern conf::item<size_t> events__room_events__cache_comp__size;
	extern const db::prefix_transform events__room_events__pfx;
	extern const db::comparator events__room_events__cmp;
	extern const db::descriptor events__room_events;
}
