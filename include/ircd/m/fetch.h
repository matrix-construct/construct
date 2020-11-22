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

/// Event Fetcher (remote).
///
/// This is a federation network interface to find and retrieve datas from
/// remote parties serially. It operates by querying servers in a room until
/// one server can provide a satisfying response. The exact method for
/// determining who to contact, when and how is encapsulated internally for
/// further development, but it is primarily stochastic. All viable servers
/// in a room are exhausted before an error is the result. A hint may be
/// provided in the options by the caller. If supplied, it will be attempted
/// first.
///
/// This is an asynchronous promise/future based interface. The result package
/// is delivered by a ctx::future with a result managing allocations that
/// originate internally. The caller of start() has no further responsibilities
/// to this interface.
///
/// Due to the composition of multiple operations performed internally,  result
/// future has no real timeout control over the operation as a whole. While it
/// can always go out of scope for an effective cancelation, internal conf::item
/// are used to timeout failures after a deterministic `timeout * servers`.
/// This means the user is not required to wait_for() or wait_until() on the
/// future unless they want a stricter timeout; that may miss a valid response
/// for a rare piece of data held by a minority of servers.
///
/// Alternatively, m::feds is another federation network interface geared to
/// conducting a parallel request to every server in a room; this conducts a
/// serial request to every server in a room (and stopping when satisfied).
///
namespace ircd::m::fetch
{
	struct init;
	struct opts;
	struct result;
	struct request;
	enum class op :uint8_t;

	// Observers
	string_view reflect(const op &);
	bool for_each(const std::function<bool (request &)> &);
	bool exists(const opts &);
	size_t count();

	// Primary operations
	ctx::future<result> start(opts);
}

enum class ircd::m::fetch::op
:uint8_t
{
	noop,
	auth,
	event,
	backfill,
};

struct ircd::m::fetch::opts
{
	/// Operation to perform.
	fetch::op op {op::noop};

	/// room::id apropos. Many federation requests require a room_id, but
	/// nevertheless a room_id is still used by this unit as a pool of servers.
	room::id room_id;

	/// event::id apropos. For op::event operations this is being sought, but
	/// for others it may be required as a reference point. If not supplied and
	/// required, we'll try to use the top head from any room_id.
	event::id event_id;

	/// The principal allocation size. This is passed up the stack to m::fed,
	/// server::request and ends up containing the request head and content,
	/// and response head. The response content is usually dynamically
	/// allocated and that buffer is the one which ends up in result. Note
	/// that sufficiently large values here may allow for eliding the content
	/// allocation based on the following formula: >= 16_KiB + (64_KiB * limit)
	/// where 16_KiB is [current server default] for headers and 64_KiB is
	/// m::event::MAX_SIZE.
	size_t bufsz {0};

	/// Name of a remote server which will be queried first; if failure,
	/// the normal room_id based operation is the fallback. If the room
	/// is not known to us, it would be best to set this.
	string_view hint;

	/// Limit the number of servers to be contacted for this operation. Zero
	/// is automatic / unlimited. Note that setting this value to 1 in
	/// conjunction with a hint is analogous to just making an m::fed request.
	size_t attempt_limit {0};

	//
	// special options
	//

	/// If the op makes use of a spec limit parameter that can be controlled
	/// by the user here. The default of 0 will be replaced by some internal
	/// configured limit like 8 or 16 etc.
	size_t backfill_limit {0};

	/// Whether to hash the result for event_id (ignored for v1/v2); this is
	/// important to ignore poisonous results and continuing.
	bool check_event_id {true};

	/// Whether to run the conforms checks on the result; this is important
	/// to screen out poisonous results while continuing to try other servers.
	bool check_conforms {true};

	/// Whether to check if the content hash matches. This might not match if
	/// the event is redacted (or junk), so other servers will then be tried.
	/// Note the case of authoriative redactions below; and if true that may
	/// allow a condition for forcing check_hashes=false.
	bool check_hashes {true};

	/// Whether to allow content hash mismatch iff the result was received from
	/// the event's origin. If the origin of the event wants to redact the
	/// event we accept; otherwise we continue to look for an unredacted copy.
	bool authoritative_redaction {true};

	/// Whether to verify signature of result before accepting; this is
	/// important to ignore poisonous results and continuing.
	bool check_signature {true};
};

struct ircd::m::fetch::result
{
	/// Backing buffer for any data pointed to by this result.
	shared_buffer<mutable_buffer> buf;

	/// The backing buffer may contain other data ahead of the response
	/// content; in any case this points to a view of the response content.
	/// User access to response content should be via a json conversion rather
	/// than this reference.
	string_view content;

	/// The name of the remote which supplied us with the result.
	char origin[rfc3986::DOMAIN_BUFSIZE];

	/// JSON result conversion. Note that developers should not let the result
	/// instance go out of scope by making this conversion.
	explicit operator json::object() const;
	explicit operator json::array() const;
};

/// Fetch entity state. DO NOT CONSTRUCT. This is an internal structure but we
/// expose it here for examination, statistics and hacking since it has no
/// non-standard symbols; this is simpler than creating some accessor suite.
/// Instances of this object are created and managed internally by the m::fetch
/// unit after a fetch::start() is called. This definition is not required to
/// operate the m::fetch interface as a user.
struct ircd::m::fetch::request
{
	using is_transparent = void;

	/// Copy of the user's request options. Note that the backing of strings in
	/// opts was changed to point at this structure; allowing safe access.
	fetch::opts opts;

	/// Time the first attempt was made; this value is not modified so it can
	/// be used to measure the total time of all attempts.
	system_point started;

	/// Time the last attempt was started
	system_point last;

	/// Time the request entered the finished state. This being non-zero
	/// indicates a finished state; may be difficult to observe.
	system_point finished;

	/// State for failed attempts; the names of servers which failed are
	/// stored here. Failure here means the request succeeded but the server
	/// did not provide a satisfying response. Appearing in this list prevents
	/// a server from being selected for the next attempt.
	std::set<std::string, std::less<>> attempted;

	/// Reference to the current server being attempted. This string is placed
	/// in the attempted set at the start of an attempt.
	string_view origin;

	/// HTTP heads and scratch buffer for server::request
	unique_buffer<mutable_buffer> buf;

	/// Our future for the server::request. Since we make
	std::unique_ptr<server::request> future;

	/// Promise for our user's future of this request.
	ctx::promise<result> promise;

	/// Error pointer state for an attempt. This is cleared each attempt.
	std::exception_ptr eptr;

	/// Buffer backing for opts
	m::event::id::buf event_id;
	m::room::id::buf room_id;

	/// Internal
	request(const fetch::opts &);
	request(request &&) = delete;
	request(const request &) = delete;
	request &operator=(request &&) = delete;
	request &operator=(const request &) = delete;
	~request() noexcept;
};

/// Internally held
struct ircd::m::fetch::init
{
	init(), ~init() noexcept;
};

inline
ircd::m::fetch::result::operator
json::array()
const
{
	return content;
}

inline
ircd::m::fetch::result::operator
json::object()
const
{
	return content;
}
