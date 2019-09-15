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
#define HAVE_IRCD_M_ROOMS_SUMMARY_H

/// Interface to tools that build a publicrooms summary from room state.
namespace ircd::m::rooms::summary
{
	struct fetch;

	// util
	string_view make_state_key(const mutable_buffer &, const room::id &);
	room::id::buf unmake_state_key(const string_view &);

	// observers
	bool has(const room::id &);
	void chunk(const room &, json::stack::object &chunk);
	json::object chunk(const room &, const mutable_buffer &out);

	// mutators
	event::id::buf set(const room::id &, const json::object &summary);
	event::id::buf set(const room &);
	event::id::buf del(const room &);
}

struct ircd::m::rooms::summary::fetch
{
	// conf
	static conf::item<size_t> limit;
	static conf::item<seconds> timeout;

	// result
	size_t total_room_count_estimate {0};
	std::string next_batch;

	// request
	fetch(const string_view &origin,
	      const string_view &since   =  {},
	      const size_t &limit        = 64);

	fetch() = default;
};
