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

	// [SET (txn)] Basic write suite
	string_view write(db::txn &, const event &, const write_opts &);
}

/// Database description
///
namespace ircd::m::dbs::desc
{
	extern const db::description events;
}

#include "appendix.h"
#include "write_opts.h"
#include "event_column.h"
#include "event_idx.h"
#include "event_json.h"
#include "event_refs.h"
#include "event_horizon.h"
#include "event_type.h"
#include "event_sender.h"
#include "room_head.h"
#include "room_events.h"
#include "room_joined.h"
#include "room_state.h"
#include "state_node.h"

struct ircd::m::dbs::init
{
	init(std::string dbopts = {});
	~init() noexcept;
};
