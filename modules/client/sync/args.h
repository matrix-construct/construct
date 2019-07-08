// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

struct ircd::m::sync::args
{
	static conf::item<milliseconds> timeout_max;
	static conf::item<milliseconds> timeout_min;
	static conf::item<milliseconds> timeout_default;

	const resource::request &request;

	string_view filter_id
	{
		// 6.2.1 The ID of a filter created using the filter API or a filter JSON object
		// encoded as a string. The server will detect whether it is an ID or a JSON object
		// by whether the first character is a "{" open brace. Passing the JSON inline is best
		// suited to one off requests. Creating a filter using the filter API is recommended
		// for clients that reuse the same filter multiple times, for example in long poll requests.
		request.query["filter"]
	};

	std::pair<string_view, string_view> since_token
	{
		// parse the since token string; this may be two numbers separated by '_'
		// or it may be one number, or none. defaults to '0' for initial_sync.
		// The second number is used as a next_batch value cookie we gave to
		// the client (used during phased polylog sync)
		split(request.query.get("since", "0"_sv), '_')
	};

	uint64_t since
	{
		// 6.2.1 A point in time to continue a sync from.
		lex_cast<uint64_t>(since_token.first)
	};

	// This is the raw (non-spec) next_batch token which can be supplied by
	// the client as an upper-bound on the window of this sync operation.
	// If this is non-empty, the value takes precedence and will be strictly
	// adhered to. Otherwise, the next_batch below may be computed by the
	// server and may be violated on longpolls.
	string_view next_batch_token
	{
		request.query.get("next_batch", since_token.second)
	};

	// This is named the same as the next_batch response value passed to the
	// client at the conclusion of the sync operation because it will literally
	// pass through this value. The next sync operation will then start at this
	// value. This token is an event_idx, like the since token. Note it may point
	// to an event that does not yet exist past-the-end.
	uint64_t next_batch
	{
		// [experimental] A upper bound to stop this sync at. This is used in
		// conjunction with `since` to provide a stable window of results. If
		// not specified the sync range is everything after `since`. NOTE that
		// this DOES NOT guarantee true idempotency in all cases and for all
		// time. But that would be nice. Many sync modules do not support this
		// because the results of repeated calls for range may become empty
		// after a while.
		uint64_t(lex_cast<int64_t>(next_batch_token?: "-1"_sv))
	};

	steady_point timesout{[this]
	{
		// 6.2.1 The maximum time to poll in milliseconds before returning this request.
		auto ret(request.query.get("timeout", milliseconds(timeout_default)));
		ret = std::min(ret, milliseconds(timeout_max));
		ret = std::max(ret, milliseconds(timeout_min));
		return now<steady_point>() + ret;
	}()};

	bool full_state
	{
		// 6.2.1 Controls whether to include the full state for all rooms the user is a member of.
		// If this is set to true, then all state events will be returned, even if since is non-empty.
		// The timeline will still be limited by the since parameter. In this case, the timeout
		// parameter will be ignored and the query will return immediately, possibly with an
		// empty timeline. If false, and since is non-empty, only state which has changed since
		// the point indicated by since will be returned. By default, this is false.
		request.query.get("full_state", false)
	};

	bool set_presence
	{
		// 6.2.1 Controls whether the client is automatically marked as online by polling this API.
		// If this parameter is omitted then the client is automatically marked as online when it
		// uses this API. Otherwise if the parameter is set to "offline" then the client is not
		// marked as being online when it uses this API. One of: ["offline"]
		request.query.get("set_presence", true)
	};

	bool phased
	{
		request.query.get("phased", true)
	};

	args(const resource::request &request);
};
