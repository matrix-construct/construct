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

	// sender keyspace
	bool is_event_sender_key(const string_view &amalgam);
	string_view event_sender_key(const mutable_buffer &out, const id::user &, const event::idx & = 0UL);
	std::tuple<event::idx> event_sender_key(const string_view &amalgam);

	// sender_origin keyspace
	bool is_event_sender_origin_key(const string_view &amalgam);
	string_view event_sender_origin_key(const mutable_buffer &out, const string_view &origin, const string_view &localpart = {}, const event::idx & = 0UL);
	string_view event_sender_origin_key(const mutable_buffer &out, const id::user &, const event::idx &);
	std::tuple<string_view, event::idx> event_sender_origin_key(const string_view &amalgam);

	void _index_event_sender(db::txn &, const event &, const write_opts &);

	// mxid | event_idx
	// host | local, event_idx  (see event_sender_origin.h)
	extern db::domain event_sender;
}

namespace ircd::m::dbs::desc
{
	extern conf::item<std::string> event_sender__comp;
	extern conf::item<size_t> event_sender__block__size;
	extern conf::item<size_t> event_sender__meta_block__size;
	extern conf::item<size_t> event_sender__cache__size;
	extern conf::item<size_t> event_sender__cache_comp__size;
	extern const db::prefix_transform event_sender__pfx;
	extern const db::descriptor event_sender;
}
