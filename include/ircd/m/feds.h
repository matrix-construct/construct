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

/// Parallel federation network interface. This fronts several of the m::fed
/// requests and conducts them to all servers in a room (e.g. m::room::origins)
/// at the same time.
///
/// This is a "hybrid" of internally asynchronous operations anchored to a
/// context by a synchronous execution device (`feds::execute`). The closure
/// is invoked asynchronously as results come in. If the closure returns false,
/// the interface function will return immediately and all pending requests will
/// go out of scope and may be cancelled as per ircd::server decides.
///
/// Alternatively, m::fetch is another federation network interface much better
/// suited to find-and-retrieve for a single piece of data (i.e an event). This
/// interface unconditionally launches requests to every server in parallel, if
/// one server's response provides a satisfying result this method can be
/// wasteful in comparison.
///
namespace ircd::m::feds
{
	enum class op :uint8_t;
	struct opts;
	struct result;
	struct execute;
	using closure = std::function<bool (const result &)>;
};

/// Execute federation operations in parallel.
///
/// This device is invoked with request options and a result closure. If
/// the user wishes to execute multiple parallel operations in parallel,
/// a vector of options can be passed. The result structure passed to the
/// user's closure contains a pointer to the related opts structure, so
/// the user can distinguish different requests in their options vector.
///
struct ircd::m::feds::execute
:boolean
{
	execute(const vector_view<const opts> &, const closure &);
	execute(const opts &, const closure &);
};

/// Supported operations.
enum class ircd::m::feds::op
:uint8_t
{
	noop,
	head,
	auth,
	event,
	state,
	backfill,
	version,
	keys,
	send,
};

/// Result structure created internally when a result arrives and passed to
/// the user's closure. The structure is merely an alternative to specifying
/// a lot of arguments to the closure.
///
struct ircd::m::feds::result
{
	/// Points at the opts passed to execute().
	const opts *request;

	/// The remote server which provided this result.
	string_view origin;

	/// Error pointer. This will contain an exception if a remote cannot be
	/// contacted, or did not return a 2xx HTTP status. When the eptr is set
	/// the result contents (below) will be empty. Note that several options
	/// control the conditions for invoking the closure with this eptr set.
	std::exception_ptr eptr;

	/// Result content. This points to successfully received result JSON from
	/// the remote; or empty if eptr is set. Note that both of these point to
	/// the same content because the user is most likely expecting one and
	/// ircd::json will just throw if trouble.
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

	/// Whether to call the user's result closure with a cached error result
	/// before the request is even made to the remote. If false (the default)
	/// the user's closure is never invoked and no request is made if a remote
	/// has a cached error.
	bool closure_cached_errors {false};

	/// Whether to skip any loopback queries to my own host. This is false by
	/// default, and loopback queries are made for result completeness in the
	/// typical use case.
	bool exclude_myself {false};

	/// Whether to iterate the query targets first to perform prelinks. This is
	/// an asynchronous operation which may perform server name resolution and
	/// link estab. The main request loop will then have fewer hazards.
	bool prelink {true};

	// Default construction is inline by member; this is defined to impose
	// noexcept over `milliseconds timeout` which we guarantee won't throw.
	opts() noexcept {}
};

inline
ircd::m::feds::execute::execute(const opts &opts,
                                const closure &closure)
:execute
{
	vector_view<const feds::opts>(std::addressof(opts), 1),
	closure
}
{}
