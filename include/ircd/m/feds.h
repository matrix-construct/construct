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
#define HAVE_IRCD_M_FEDS_H

/// Concurrent federation request interface. This fronts several of the m::v1
/// requests and conducts them to all servers in a room (e.g. m::room::origins)
/// at the same time. The hybrid control flow of this interface is best suited
/// to the real-world uses of these operations.
///
/// Each call in this interface is synchronous and will block the ircd::ctx
/// until it returns. The return value is the for_each-protocol result based on your
/// closure (if the closure ever returns false, the function also returns false).
///
/// The closure is invoked asynchronously as results come in. If the closure
/// returns false, the interface function will return immediately and all
/// pending requests will go out of scope and may be cancelled as per
/// ircd::server decides.
namespace ircd::m::feds
{
	enum class op :uint8_t;
	struct opts;
	struct result;
	struct acquire;
	using closure = std::function<bool (const result &)>;
};

struct ircd::m::feds::acquire
{
	acquire(const vector_view<const opts> &, const closure &);
	acquire(const opts &, const closure &);
};

struct ircd::m::feds::result
{
	const opts *request;
	string_view origin;
	std::exception_ptr eptr;
	json::object object;
	json::array array;
};

struct ircd::m::feds::opts
{
	/// Operation type
	enum op op {(enum op)0};

	/// Timeout for this operation. For a batch of operations, this system
	/// may attempt -- but does not guarantee -- to cancel timed-out requests
	/// before the highest timeout value in the batch.
	milliseconds timeout {20000L};

	/// Apropos room_id: this is almost always required for this interface
	/// because the servers in the room is used for the request target set.
	m::room::id room_id;

	/// Apropos event_id for several operations.
	m::event::id event_id;

	/// Apropos user_id for several operations.
	m::user::id user_id;

	/// Misc string argument registers. These convey values for special
	/// features in individual operations.
	string_view arg[4];  // misc argv

	/// Misc integer argument registers. These convey values for special
	/// features in individual operations.
	uint64_t argi[4];    // misc integer argv

	/// Whether exceptions from the supplied result closure are propagated.
	bool nothrow_closure {false};

	/// When nothrow_closure is true, this determines whether or not to
	/// continue receiving results or to break and return. True to continue.
	bool nothrow_closure_retval {true};

	/// Whether to call the user's result closure for error results, which
	/// would have the eptr set. When this is false, the closure is never
	/// invoked with eptr set and nothrow_closure_retval is used to continue.
	bool closure_errors {true};
};

enum class ircd::m::feds::op
:uint8_t
{
	noop,
	head,
	auth ,
	event,
	state,
	backfill,
	version,
	keys,
	send,
};
