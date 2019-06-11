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
#define HAVE_IRCD_M_DBS_ROOM_STATE_H

namespace ircd::m::dbs
{
	constexpr size_t ROOM_STATE_KEY_MAX_SIZE
	{
		id::MAX_SIZE + event::TYPE_MAX_SIZE + event::STATE_KEY_MAX_SIZE
	};

	string_view room_state_key(const mutable_buffer &out, const id::room &, const string_view &type, const string_view &state_key);
	string_view room_state_key(const mutable_buffer &out, const id::room &, const string_view &type);
	std::pair<string_view, string_view> room_state_key(const string_view &amalgam);

	// room_id | type, state_key => event_idx
	extern db::domain room_state;
}

namespace ircd::m::dbs::desc
{
	extern conf::item<size_t> events__room_state__block__size;
	extern conf::item<size_t> events__room_state__meta_block__size;
	extern conf::item<size_t> events__room_state__cache__size;
	extern conf::item<size_t> events__room_state__cache_comp__size;
	extern conf::item<size_t> events__room_state__bloom__bits;
	extern const db::prefix_transform events__room_state__pfx;
	extern const db::descriptor events__room_state;
}
