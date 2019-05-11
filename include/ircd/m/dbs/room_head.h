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
#define HAVE_IRCD_M_DBS_ROOM_HEAD_H

namespace ircd::m::dbs
{
	constexpr size_t ROOM_HEAD_KEY_MAX_SIZE
	{
		id::MAX_SIZE + 1 + id::MAX_SIZE
	};

	string_view room_head_key(const mutable_buffer &out, const id::room &, const id::event &);
	string_view room_head_key(const string_view &amalgam);

	// room_id | event_id => event_idx
	extern db::index room_head;
}

namespace ircd::m::dbs::desc
{
	extern conf::item<size_t> events__room_head__block__size;
	extern conf::item<size_t> events__room_head__meta_block__size;
	extern conf::item<size_t> events__room_head__cache__size;
	extern const db::prefix_transform events__room_head__pfx;
	extern const db::descriptor events__room_head;
}
