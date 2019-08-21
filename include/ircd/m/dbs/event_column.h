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
#define HAVE_IRCD_M_DBS_EVENT_COLUMN_H

// These columns all store duplicate data from the original _event_json
// but limited to a single specific property. The index key is an event_idx
// (just like _event_json). These columns are useful for various optimizations
// at the cost of the additional space consumed.

namespace ircd::m::dbs
{
	// Event property column max-count. The number of event columns may be
	// less by not initializing positions in the event_column array.
	constexpr const auto event_columns
	{
		event::size()
	};

	// There is one position in this array corresponding to each property
	// in the m::event tuple, however, the db::column in this position may
	// be default-initialized if this column is not used.
	extern std::array<db::column, event_columns> event_column;
}

namespace ircd::m::dbs::desc
{
	extern conf::item<size_t> events___event__bloom__bits;

	extern conf::item<size_t> events__content__block__size;
	extern conf::item<size_t> events__content__meta_block__size;
	extern conf::item<size_t> events__content__cache__size;
	extern conf::item<size_t> events__content__cache_comp__size;
	extern const db::descriptor events_content;

	extern conf::item<size_t> events__depth__block__size;
	extern conf::item<size_t> events__depth__meta_block__size;
	extern conf::item<size_t> events__depth__cache__size;
	extern conf::item<size_t> events__depth__cache_comp__size;
	extern const db::descriptor events_depth;

	extern conf::item<size_t> events__event_id__block__size;
	extern conf::item<size_t> events__event_id__meta_block__size;
	extern conf::item<size_t> events__event_id__cache__size;
	extern conf::item<size_t> events__event_id__cache_comp__size;
	extern const db::descriptor events_event_id;

	extern conf::item<size_t> events__origin_server_ts__block__size;
	extern conf::item<size_t> events__origin_server_ts__meta_block__size;
	extern conf::item<size_t> events__origin_server_ts__cache__size;
	extern conf::item<size_t> events__origin_server_ts__cache_comp__size;
	extern const db::descriptor events_origin_server_ts;

	extern conf::item<size_t> events__room_id__block__size;
	extern conf::item<size_t> events__room_id__meta_block__size;
	extern conf::item<size_t> events__room_id__cache__size;
	extern conf::item<size_t> events__room_id__cache_comp__size;
	extern const db::descriptor events_room_id;

	extern conf::item<size_t> events__sender__block__size;
	extern conf::item<size_t> events__sender__meta_block__size;
	extern conf::item<size_t> events__sender__cache__size;
	extern conf::item<size_t> events__sender__cache_comp__size;
	extern const db::descriptor events_sender;

	extern conf::item<size_t> events__state_key__block__size;
	extern conf::item<size_t> events__state_key__meta_block__size;
	extern conf::item<size_t> events__state_key__cache__size;
	extern conf::item<size_t> events__state_key__cache_comp__size;
	extern const db::descriptor events_state_key;

	extern conf::item<size_t> events__type__block__size;
	extern conf::item<size_t> events__type__meta_block__size;
	extern conf::item<size_t> events__type__cache__size;
	extern conf::item<size_t> events__type__cache_comp__size;
	extern const db::descriptor events_type;
}
