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
#define HAVE_IRCD_M_DBS_EVENT_JSON_H

namespace ircd::m::dbs
{
	// event_idx => full json
	extern db::column event_json;
}

namespace ircd::m::dbs::desc
{
	extern conf::item<size_t> events__event_json__block__size;
	extern conf::item<size_t> events__event_json__meta_block__size;
	extern conf::item<size_t> events__event_json__cache__size;
	extern conf::item<size_t> events__event_json__cache_comp__size;
	extern conf::item<size_t> events__event_json__bloom__bits;
	extern const db::descriptor events__event_json;
}
