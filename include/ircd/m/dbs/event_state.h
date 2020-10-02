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
#define HAVE_IRCD_M_DBS_EVENT_STATE_H

namespace ircd::m::dbs
{
	constexpr size_t EVENT_STATE_KEY_MAX_SIZE
	{
		0
		+ event::STATE_KEY_MAX_SIZE
		+ event::TYPE_MAX_SIZE
		+ id::MAX_SIZE
		+ 1
		+ 8
		+ 8
	};

	// state_key, type, room_id, depth, event_idx
	using event_state_tuple = std::tuple<string_view, string_view, string_view, int64_t, event::idx>;

	string_view event_state_key(const mutable_buffer &out, const event_state_tuple &);
	event_state_tuple event_state_key(const string_view &);

	void _index_event_state(db::txn &, const event &, const write_opts &);

	// state_key, type, room_id, depth, event_idx
	extern db::domain event_state;
}

namespace ircd::m::dbs::desc
{
	// events _event_state
	extern conf::item<std::string> event_state__comp;
	extern conf::item<size_t> event_state__block__size;
	extern conf::item<size_t> event_state__meta_block__size;
	extern conf::item<size_t> event_state__cache__size;
	extern conf::item<size_t> event_state__cache_comp__size;
	extern const db::comparator event_state__cmp;
	extern const db::descriptor event_state;
}
