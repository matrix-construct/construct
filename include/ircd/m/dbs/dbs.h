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
#define HAVE_IRCD_M_DBS_DBS_H

/// Database Schema
namespace ircd::m::dbs
{
	struct init;
	struct opts;
	enum class ref :uint8_t;

	// General confs
	extern conf::item<bool> cache_enable;
	extern conf::item<bool> cache_comp_enable;
	extern conf::item<bool> prefetch_enable;
	extern conf::item<size_t> mem_write_buffer_size;
	extern conf::item<size_t> sst_write_buffer_size;

	// Database instance
	extern std::shared_ptr<db::database> events;

	// [SET (txn)] Basic write suite
	size_t prefetch(const event &, const opts &);
	size_t write(db::txn &, const event &, const opts &);
}

/// Database description
namespace ircd::m::dbs::desc
{
	extern const db::description events;
}

/// Transaction appendage builder namespace.
namespace ircd::m::dbs::appendix
{
	enum index :uint8_t;
}

#include "event_idx.h"              // event_id => event_idx
#include "event_json.h"             // event_idx => (full JSON)
#include "event_column.h"           // event_idx => (direct value)
#include "event_refs.h"             // event_idx | ref_type, event_idx
#include "event_horizon.h"          // event_id | event_idx
#include "event_sender.h"           // sender | event_idx || hostpart | localpart, event_idx
#include "event_type.h"             // type | event_idx
#include "event_state.h"            // state_key, type, room_id, depth, event_idx
#include "room_events.h"            // room_id | depth, event_idx
#include "room_type.h"              // room_id | type, depth, event_idx
#include "room_state.h"             // room_id | type, state_key => event_idx
#include "room_state_space.h"       // room_id | type, state_key, depth, event_idx
#include "room_joined.h"            // room_id | origin, member => event_idx
#include "room_head.h"              // room_id | event_id => event_idx
#include "init.h"
#include "opts.h"

// Some internal utils (here for now)
namespace ircd::m::dbs
{
	size_t prefetch_event_idx(const vector_view<const event::id> &in, const opts &);
	bool prefetch_event_idx(const event::id &, const opts &);

	size_t find_event_idx(const vector_view<event::idx> &out, const vector_view<const event::id> &in, const opts &);
	event::idx find_event_idx(const event::id &, const opts &);
}

inline ircd::m::event::idx
ircd::m::dbs::find_event_idx(const event::id &event_id,
                             const opts &wopts)
{
	event::idx ret{0};
	const vector_view<event::idx> out(&ret, 1);
	const vector_view<const event::id> in(&event_id, 1);
	find_event_idx(out, in, wopts);
	return ret;
}

inline bool
ircd::m::dbs::prefetch_event_idx(const event::id &event_id,
                                 const opts &wopts)
{
	const vector_view<const event::id> in(&event_id, 1);
	return prefetch_event_idx(in, wopts);
}
