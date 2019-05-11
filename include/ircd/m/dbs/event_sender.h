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
#define HAVE_IRCD_M_DBS_EVENT_SENDER_H

namespace ircd::m::dbs
{
	constexpr size_t EVENT_SENDER_KEY_MAX_SIZE
	{
		id::MAX_SIZE + 1 + 8
	};

	string_view event_sender_key(const mutable_buffer &out, const string_view &origin, const string_view &localpart = {}, const event::idx & = 0);
	string_view event_sender_key(const mutable_buffer &out, const id::user &, const event::idx &);
	std::tuple<string_view, event::idx> event_sender_key(const string_view &amalgam);

	// host | local, event_idx
	extern db::index event_sender;
}

namespace ircd::m::dbs::desc
{
	extern conf::item<size_t> events__event_sender__block__size;
	extern conf::item<size_t> events__event_sender__meta_block__size;
	extern conf::item<size_t> events__event_sender__cache__size;
	extern conf::item<size_t> events__event_sender__cache_comp__size;
	extern const db::prefix_transform events__event_sender__pfx;
	extern const db::descriptor events__event_sender;
}
