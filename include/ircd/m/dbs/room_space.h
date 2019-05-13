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
#define HAVE_IRCD_M_DBS_ROOM_SPACE_H

namespace ircd::m::dbs
{
	using room_space_key_parts = std::tuple<string_view, string_view, int64_t, event::idx>;

	constexpr size_t ROOM_SPACE_KEY_MAX_SIZE
	{
		id::MAX_SIZE +
		event::TYPE_MAX_SIZE +
		event::STATE_KEY_MAX_SIZE +
		sizeof(int64_t) +
		sizeof(event::idx)
	};

	string_view room_space_key(const mutable_buffer &out, const id::room &, const string_view &type, const string_view &state_key, const int64_t &depth, const event::idx & = 0);
	string_view room_space_key(const mutable_buffer &out, const id::room &, const string_view &type, const string_view &state_key);
	string_view room_space_key(const mutable_buffer &out, const id::room &, const string_view &type);
	room_space_key_parts room_space_key(const string_view &amalgam);

	// room_id | type, state_key, depth, event_idx => --
	extern db::index room_space;
}

namespace ircd::m::dbs::desc
{
	extern conf::item<size_t> events__room_space__block__size;
	extern conf::item<size_t> events__room_space__meta_block__size;
	extern conf::item<size_t> events__room_space__cache__size;
	extern conf::item<size_t> events__room_space__cache_comp__size;
	extern conf::item<size_t> events__room_space__bloom__bits;
	extern const db::prefix_transform events__room_space__pfx;
	extern const db::comparator events__room_space__cmp;
	extern const db::descriptor events__room_space;
}
