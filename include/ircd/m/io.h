// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_IO_H

/// Interface to the matrix protocol IO bus making local and network queries.
///
/// This system is the backplane for the m::vm or anything else that needs to
/// get events, however it can, as best as it can, at a high level using a
/// convenient interface. Users of this interface fill out and maintain a
/// control structure (or several) on their stack and then make calls which may
/// yield their ircd::ctx with report given in the control structure. The
/// default behavior will try to hide all of the boilerplate from the user
/// when it comes to figuring out where to make a query and then verifying the
/// results. The control structure offers the ability to tailor very low level
/// aspects of the request and change behavior if desired.
///
/// For acquisition, this interface provides the means to find an event, or
/// set of events by first querying the local db and caches and then making
/// network queries using the matrix protocol endpoints.
///
/// For transmission, this interface provides the means to send events et al
/// to other servers; no local/database writes will happen here, just network.
///
/// There are several variations of requests to the bus; each reflects the matrix
/// protocol endpoint which is apt to best fulfill the request.
///
/// * fetch event       - request for event by ID (/event)
/// * fetch room        - request for vector of room events (/backfill)
/// * fetch room state  - request for set of state events (/state)
///
/// Unless the control structure specifies otherwise, result data for these
/// requests may be filled entirely locally, remotely, or partially from
/// either.
///
namespace ircd::m::io
{
	struct sync;
	struct fetch;
	struct response;

	// Synchronous acquire many requests
	size_t acquire(vector_view<event::fetch>);
	size_t acquire(vector_view<room::fetch>);
	size_t acquire(vector_view<room::state::fetch>);

	// Synchronous acquire single request
	json::object acquire(event::fetch &);
	json::array acquire(room::fetch &);
	json::array acquire(room::state::fetch &);

	// Synchronous release
	size_t release(vector_view<event::sync>);
	void release(event::sync &);

	// Convenience
	json::object get(const event::id &, const mutable_buffer &);
}

namespace ircd::m
{
	using io::get;
	using io::response;
}

//
// Fetch & Sync base
//

struct ircd::m::io::fetch
{
	struct opts;

	static const opts defaults;

	mutable_buffer buf;
	const struct opts *opts {&defaults};
	bool local_result {false};
	std::exception_ptr error;
};

struct ircd::m::io::fetch::opts
{
	net::remote hint;
	uint64_t limit {256};
};

struct ircd::m::io::sync
{
	struct opts;

	static const opts defaults;

	const_buffer buf;
	const struct opts *opts {&defaults};
	std::exception_ptr error;
};

struct ircd::m::io::sync::opts
{
	net::remote hint;
};

//
// Event
//

struct ircd::m::event::fetch
:io::fetch
{
	// out
	event::id event_id;

	// in
	json::object pdu;

	fetch(const event::id &event_id,
	      const mutable_buffer &buf,
	      const struct opts *const &opts = &defaults)
	:io::fetch{buf, opts}
	,event_id{event_id}
	{}

	fetch() = default;
};

struct ircd::m::event::sync
:io::sync
{
	// out
	string_view destination;
	uint64_t txnid {0};

	// in
	std::exception_ptr error;


	sync(const string_view &destination,
	     const const_buffer &buf,
	     const struct opts *const &opts = &defaults)
	:io::sync{buf, opts}
	,destination{destination}
	{}

	sync() = default;
};

//
// Room (backfill)
//

struct ircd::m::room::fetch
:io::fetch
{
	// out
	event::id event_id;
	room::id room_id;

	// in
	json::array pdus;
	json::array auth_chain;

	fetch(const event::id &event_id,
	      const room::id &room_id,
	      const mutable_buffer &buf,
	      const struct opts *const &opts = &defaults)
	:io::fetch{buf, opts}
	,event_id{event_id}
	,room_id{room_id}
	{}

	fetch() = default;
};

//
// Room (state)
//

struct ircd::m::room::state::fetch
:io::fetch
{
	// out
	event::id event_id;
	room::id room_id;

	// in
	json::array pdus;
	json::array auth_chain;

	fetch(const event::id &event_id,
	      const room::id &room_id,
	      const mutable_buffer &buf,
	      const struct opts *const &opts = &defaults)
	:io::fetch{buf, opts}
	,event_id{event_id}
	,room_id{room_id}
	{}

	fetch() = default;
};

struct ircd::m::io::response
:json::object
{
	response(server::request &, parse::buffer &);
};
