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
	using closure = std::function<bool (const string_view &, const json::object &)>;
	using closure_idx = std::function<bool (const string_view &, const event::idx &)>;

	string_view make_state_key(const mutable_buffer &, const room::id &, const string_view &origin);
	std::pair<room::id, string_view> unmake_state_key(const string_view &);

	bool for_each(const room::id &, const closure_idx &);
	bool for_each(const room::id &, const closure &);

	bool has(const room::id &, const string_view &origin = {});

	void get(json::stack::object &chunk, const room &);
	json::object get(const mutable_buffer &out, const room &);

	event::id::buf set(const room::id &, const string_view &origin, const json::object &summary);
	event::id::buf set(const room &);

	event::id::buf del(const room &, const string_view &origin);
	void del(const room &);
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
	      const size_t &limit        = 64,
	      const string_view &search  = {});

	fetch() = default;
};
