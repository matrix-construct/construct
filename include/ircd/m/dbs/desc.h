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
#define HAVE_IRCD_M_DBS_DESC_H

/// Database Schema Descriptors
///
namespace ircd::m::dbs::desc
{
	// Full description
	extern const db::description events;

	//
	// Direct columns
	//

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

	//
	// Metadata columns
	//

	// events index
	extern conf::item<size_t> events__event_idx__block__size;
	extern conf::item<size_t> events__event_idx__meta_block__size;
	extern conf::item<size_t> events__event_idx__cache__size;
	extern conf::item<size_t> events__event_idx__cache_comp__size;
	extern conf::item<size_t> events__event_idx__bloom__bits;
	extern const db::descriptor events__event_idx;

	// events cache
	extern conf::item<size_t> events__event_json__block__size;
	extern conf::item<size_t> events__event_json__meta_block__size;
	extern conf::item<size_t> events__event_json__cache__size;
	extern conf::item<size_t> events__event_json__cache_comp__size;
	extern conf::item<size_t> events__event_json__bloom__bits;
	extern const db::descriptor events__event_json;

	// event horizon
	extern conf::item<size_t> events__event_horizon__block__size;
	extern conf::item<size_t> events__event_horizon__meta_block__size;
	extern conf::item<size_t> events__event_horizon__cache__size;
	extern conf::item<size_t> events__event_horizon__cache_comp__size;
	extern const db::prefix_transform events__event_horizon__pfx;
	extern const db::descriptor events__event_horizon;

	// events sender
	extern conf::item<size_t> events__event_sender__block__size;
	extern conf::item<size_t> events__event_sender__meta_block__size;
	extern conf::item<size_t> events__event_sender__cache__size;
	extern conf::item<size_t> events__event_sender__cache_comp__size;
	extern const db::prefix_transform events__event_sender__pfx;
	extern const db::descriptor events__event_sender;

	// events type
	extern conf::item<size_t> events__event_type__block__size;
	extern conf::item<size_t> events__event_type__meta_block__size;
	extern conf::item<size_t> events__event_type__cache__size;
	extern conf::item<size_t> events__event_type__cache_comp__size;
	extern const db::prefix_transform events__event_type__pfx;
	extern const db::descriptor events__event_type;

	// room head mapping sequence
	extern conf::item<size_t> events__room_head__block__size;
	extern conf::item<size_t> events__room_head__meta_block__size;
	extern conf::item<size_t> events__room_head__cache__size;
	extern const db::prefix_transform events__room_head__pfx;
	extern const db::descriptor events__room_head;

	// room events sequence
	extern conf::item<size_t> events__room_events__block__size;
	extern conf::item<size_t> events__room_events__meta_block__size;
	extern conf::item<size_t> events__room_events__cache__size;
	extern conf::item<size_t> events__room_events__cache_comp__size;
	extern const db::prefix_transform events__room_events__pfx;
	extern const db::comparator events__room_events__cmp;
	extern const db::descriptor events__room_events;

	// room present joined members sequence
	extern conf::item<size_t> events__room_joined__block__size;
	extern conf::item<size_t> events__room_joined__meta_block__size;
	extern conf::item<size_t> events__room_joined__cache__size;
	extern conf::item<size_t> events__room_joined__cache_comp__size;
	extern conf::item<size_t> events__room_joined__bloom__bits;
	extern const db::prefix_transform events__room_joined__pfx;
	extern const db::descriptor events__room_joined;

	// room present state mapping sequence
	extern conf::item<size_t> events__room_state__block__size;
	extern conf::item<size_t> events__room_state__meta_block__size;
	extern conf::item<size_t> events__room_state__cache__size;
	extern conf::item<size_t> events__room_state__cache_comp__size;
	extern conf::item<size_t> events__room_state__bloom__bits;
	extern const db::prefix_transform events__room_state__pfx;
	extern const db::descriptor events__room_state;

	// state btree node key-value store
	extern conf::item<size_t> events__state_node__block__size;
	extern conf::item<size_t> events__state_node__meta_block__size;
	extern conf::item<size_t> events__state_node__cache__size;
	extern conf::item<size_t> events__state_node__cache_comp__size;
	extern conf::item<size_t> events__state_node__bloom__bits;
	extern const db::descriptor events__state_node;
}
