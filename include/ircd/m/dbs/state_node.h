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
#define HAVE_IRCD_M_DBS_STATE_NODE_H

namespace ircd::m::dbs
{
	// [GET] the state root for an event (with as much information as you have)
	string_view state_root(const mutable_buffer &out, const id::room &, const event::idx &, const uint64_t &depth);
	string_view state_root(const mutable_buffer &out, const id::room &, const event::id &, const uint64_t &depth);
	string_view state_root(const mutable_buffer &out, const id::room &, const event::idx &);
	string_view state_root(const mutable_buffer &out, const id::room &, const event::id &);
	string_view state_root(const mutable_buffer &out, const event::idx &);
	string_view state_root(const mutable_buffer &out, const event::id &);
	string_view state_root(const mutable_buffer &out, const event &);

	// node_id => state::node
	extern db::column state_node;
}

namespace ircd::m::dbs::desc
{
	extern conf::item<size_t> events__state_node__block__size;
	extern conf::item<size_t> events__state_node__meta_block__size;
	extern conf::item<size_t> events__state_node__cache__size;
	extern conf::item<size_t> events__state_node__cache_comp__size;
	extern conf::item<size_t> events__state_node__bloom__bits;
	extern const db::descriptor events__state_node;
}
