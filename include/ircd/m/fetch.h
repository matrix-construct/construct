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
#define HAVE_IRCD_M_FETCH_H

/// Event Fetcher (remote). This is probably not the interface you want
/// because there is no ordinary reason for developers fetch a remote event
/// directly. This is an interface to the low-level fetch system.
///
namespace ircd::m::fetch
{
	struct request;

	// Observers
	bool for_each(const std::function<bool (request &)> &);
	bool exists(const m::event::id &);

	// Control panel
	bool cancel(request &);
	void start(const m::room::id &, const m::event::id &);
	bool prefetch(const m::room::id &, const m::event::id &);

	// Composed operations
	void auth_chain(const room &, const net::hostport &);
	void state_ids(const room &, const net::hostport &);
	void state_ids(const room &);
	void backfill(const room &, const net::hostport &);
	void backfill(const room &);
	void headfill(const room &);

	extern log::log log;
}

/// Fetch entity state. This is not meant for construction by users of this
/// interface.
struct ircd::m::fetch::request
:m::v1::event
{
	using is_transparent = void;

	m::room::id::buf room_id;
	m::event::id::buf event_id;
	unique_buffer<mutable_buffer> buf;
	std::set<std::string, std::less<>> attempted;
	string_view origin;
	time_t started {0};
	time_t last {0};
	time_t finished {0};
	std::exception_ptr eptr;

	request(const m::room::id &, const m::event::id &, const size_t &bufsz = 8_KiB);
	request(request &&) = delete;
	request(const request &) = delete;
	request &operator=(request &&) = delete;
	request &operator=(const request &) = delete;
};
