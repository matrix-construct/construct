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
#define HAVE_IRCD_M_DBS_EVENT_IDX_H

namespace ircd::m::dbs
{
	void _index_event_id(db::txn &, const event &, const write_opts &);

	extern db::column event_idx;       // event_id => event_idx
}

namespace ircd::m::dbs::desc
{
	extern conf::item<std::string> event_idx__comp;
	extern conf::item<size_t> event_idx__block__size;
	extern conf::item<size_t> event_idx__meta_block__size;
	extern conf::item<size_t> event_idx__cache__size;
	extern conf::item<size_t> event_idx__cache_comp__size;
	extern conf::item<size_t> event_idx__bloom__bits;
	extern const db::descriptor event_idx;
}
