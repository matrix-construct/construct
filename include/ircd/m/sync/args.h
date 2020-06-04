// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_SYNC_ARGS_H

namespace ircd::m::sync
{
	struct args;
}

/// Argument data constructed from the query parameters (and defaults) supplied
/// by a client hitting /sync. A pointer to this structure is generally
/// included as part of the sync::data provided to items.
struct ircd::m::sync::args
{
	static conf::item<milliseconds> timeout_max;
	static conf::item<milliseconds> timeout_min;
	static conf::item<milliseconds> timeout_default;

	/// 6.2.1 The ID of a filter created using the filter API or a filter JSON object
	/// encoded as a string. The server will detect whether it is an ID or a JSON object
	/// by whether the first character is a "{" open brace. Passing the JSON inline is best
	/// suited to one off requests. Creating a filter using the filter API is recommended
	/// for clients that reuse the same filter multiple times, for example in long poll requests.
	string_view filter_id;

	/// 6.2.1 A point in time to continue a sync from.
	/// Parse the since token string; this may be two numbers separated by '_'
	/// or it may be one number, or none. defaults to '0' for initial_sync.
	/// The second number is used as a next_batch value cookie we gave to
	/// the client (used during phased polylog sync)
	sync::since since;

	/// If this is non-empty, the value takes precedence and will be strictly
	/// adhered to. Otherwise, the next_batch below may be computed by the
	/// server and may be violated on longpolls. This is named the same as the
	/// next_batch response value passed to the client at the conclusion of the
	/// sync operation because it will literally pass through this value. The
	/// next sync operation will then start at this value. This token is an
	/// event_idx, like the since token. Note it may point to an event that
	/// does not yet exist past-the-end.
	uint64_t next_batch;

	/// The point in time at which this /sync should stop longpolling and return
	/// an empty'ish response to the client.
	system_point timesout;

	/// 6.2.1 Controls whether to include the full state for all rooms the user is a member of.
	/// If this is set to true, then all state events will be returned, even if since is non-empty.
	/// The timeline will still be limited by the since parameter. In this case, the timeout
	/// parameter will be ignored and the query will return immediately, possibly with an
	/// empty timeline. If false, and since is non-empty, only state which has changed since
	/// the point indicated by since will be returned. By default, this is false.
	bool full_state;

	/// 6.2.1 Controls whether the client is automatically marked as online by polling this API.
	/// If this parameter is omitted then the client is automatically marked as online when it
	/// uses this API. Otherwise if the parameter is set to "offline" then the client is not
	/// marked as being online when it uses this API. One of: ["offline"]
	bool set_presence;

	/// (non-spec) Controls whether to enable phased-polylog-initial-sync, also
	/// known as Crazy-Loading. This is enabled by default, but a query string
	/// of `?phased=0` will disable it for synapse-like behavior.
	bool phased;

	/// (non-spec) If this is set to true, the only response content from /sync
	/// will be a `next_batch` token. This is useful for clients that only want
	/// to use /sync as a semaphore notifying about new activity, but will
	/// retrieve the actual data another way.
	bool semaphore;

	/// Constructed by the GET /sync request method handler on its stack.
	args(const ircd::resource::request &request);
};
