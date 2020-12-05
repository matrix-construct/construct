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
#define HAVE_IRCD_M_DBS_EVENT_HORIZON_H

namespace ircd::m::dbs
{
	constexpr size_t EVENT_HORIZON_KEY_MAX_SIZE
	{
		id::MAX_SIZE + 1 + 8
	};

	string_view event_horizon_key(const mutable_buffer &out, const id::event &, const event::idx &);
	string_view event_horizon_key(const mutable_buffer &out, const id::event &);
	std::tuple<event::idx> event_horizon_key(const string_view &amalgam);

	size_t _prefetch_event_horizon_resolve(const event &, const write_opts &);
	void _index_event_horizon_resolve(db::txn &, const event &, const write_opts &); //query
	void _index_event_horizon(db::txn &, const event &, const write_opts &, const id::event &);

	// event_id | event_idx
	extern db::domain event_horizon;
}

namespace ircd::m::dbs::desc
{
	extern conf::item<std::string> event_horizon__comp;
	extern conf::item<size_t> event_horizon__block__size;
	extern conf::item<size_t> event_horizon__meta_block__size;
	extern conf::item<size_t> event_horizon__cache__size;
	extern conf::item<size_t> event_horizon__cache_comp__size;
	extern const db::prefix_transform event_horizon__pfx;
	extern const db::descriptor event_horizon;
}
