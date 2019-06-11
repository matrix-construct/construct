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
#define HAVE_IRCD_M_DBS_ROOM_JOINED_H

namespace ircd::m::dbs
{
	constexpr size_t ROOM_JOINED_KEY_MAX_SIZE
	{
		id::MAX_SIZE + event::ORIGIN_MAX_SIZE + id::MAX_SIZE
	};

	string_view room_joined_key(const mutable_buffer &out, const id::room &, const string_view &origin, const id::user &member);
	string_view room_joined_key(const mutable_buffer &out, const id::room &, const string_view &origin);
	std::pair<string_view, string_view> room_joined_key(const string_view &amalgam);

	// room_id | origin, member => event_idx
	extern db::domain room_joined;
}

namespace ircd::m::dbs::desc
{
	extern conf::item<size_t> events__room_joined__block__size;
	extern conf::item<size_t> events__room_joined__meta_block__size;
	extern conf::item<size_t> events__room_joined__cache__size;
	extern conf::item<size_t> events__room_joined__cache_comp__size;
	extern conf::item<size_t> events__room_joined__bloom__bits;
	extern const db::prefix_transform events__room_joined__pfx;
	extern const db::descriptor events__room_joined;
}
