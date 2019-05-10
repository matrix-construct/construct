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
///
namespace ircd::m::dbs
{
	struct init;
	struct write_opts;
	enum class ref :uint8_t;

	// General confs
	extern conf::item<bool> events_cache_enable;
	extern conf::item<bool> events_cache_comp_enable;
	extern conf::item<size_t> events_mem_write_buffer_size;
	extern conf::item<size_t> events_sst_write_buffer_size;

	// Database instance
	extern std::shared_ptr<db::database> events;

	// Event property columns
	constexpr const auto event_columns{event::size()};
	extern std::array<db::column, event_columns> event_column;

	// Event metadata columns
	extern db::column event_idx;       // event_id => event_idx
	extern db::column event_json;      // event_idx => full json
	extern db::index event_refs;       // event_idx | ref_type, event_idx
	extern db::index event_horizon;    // event_id | event_idx
	extern db::index event_type;       // type | event_idx
	extern db::index event_sender;     // host | local, event_idx
	extern db::index room_head;        // room_id | event_id => event_idx
	extern db::index room_events;      // room_id | depth, event_idx => node_id
	extern db::index room_joined;      // room_id | origin, member => event_idx
	extern db::index room_state;       // room_id | type, state_key => event_idx
	extern db::column state_node;      // node_id => state::node

	// [SET (txn)] Basic write suite
	string_view write(db::txn &, const event &, const write_opts &);
	void blacklist(db::txn &, const event::id &, const write_opts &);
}

#include "event_refs.h"
#include "appendix.h"
#include "write_opts.h"
#include "util.h"
#include "desc.h"

struct ircd::m::dbs::init
{
	init(std::string dbopts = {});
	~init() noexcept;
};
