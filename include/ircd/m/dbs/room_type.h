// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_DBS_ROOM_TYPE_H

namespace ircd::m::dbs
{
	constexpr size_t ROOM_TYPE_KEY_MAX_SIZE
	{
		id::MAX_SIZE                   // room_id
		+ 1                            // \0
		+ event::TYPE_MAX_SIZE         // type
		+ 1                            // \0
		+ 8                            // u64
		+ 8                            // u64
	};

	using room_type_tuple = std::tuple<string_view, uint64_t, event::idx>;

	room_type_tuple
	room_type_key(const string_view &amalgam);

	string_view
	room_type_key(const mutable_buffer &out,
	              const id::room &,
	              const string_view &type     = {},
	              const uint64_t &depth       = -1,
	              const event::idx &          = -1);

	void _index_room_type(db::txn &,  const event &, const write_opts &);

	// room_id | type, depth, event_idx
	extern db::domain room_type;
}

namespace ircd::m::dbs::desc
{
	// room events sequence
	extern conf::item<std::string> room_type__comp;
	extern conf::item<size_t> room_type__block__size;
	extern conf::item<size_t> room_type__meta_block__size;
	extern conf::item<size_t> room_type__cache__size;
	extern conf::item<size_t> room_type__cache_comp__size;
	extern const db::prefix_transform room_type__pfx;
	extern const db::comparator room_type__cmp;
	extern const db::descriptor room_type;
}
