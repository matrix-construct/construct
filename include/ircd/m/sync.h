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
#define HAVE_IRCD_M_SYNC_H

namespace ircd
{
	struct client;
}

namespace ircd::m::sync
{
	struct args;
	struct stats;
	struct data;
	struct item;
	using item_closure = std::function<void (item &)>;
	using item_closure_bool = std::function<bool (item &)>;

	string_view loghead(const data &);

	string_view make_since(const mutable_buffer &, const int64_t &);
	string_view make_since(const mutable_buffer &, const m::events::range &);

	bool apropos(const data &, const event::idx &);
	bool apropos(const data &, const event::id &);
	bool apropos(const data &, const event &);

	bool for_each(const string_view &prefix, const item_closure_bool &);
	bool for_each(const item_closure_bool &);

	extern log::log log;
	extern ctx::pool pool;
	extern conf::item<bool> stats_info;
}

struct ircd::m::sync::item
:instance_multimap<std::string, item, std::less<>>
{
	using handle = std::function<bool (data &)>;

	std::string conf_name[2];
	conf::item<bool> enable;
	conf::item<bool> stats_debug;
	handle _polylog;
	handle _linear;
	json::strung feature;
	json::object opts;
	bool phased;
	bool prefetch;

  public:
	string_view name() const;
	string_view member_name() const;
	size_t children() const;

	bool linear(data &);
	bool polylog(data &);

	item(std::string name,
	     handle polylog         = {},
	     handle linear          = {},
	     const json::members &  = {});

	item(item &&) = delete;
	item(const item &) = delete;
	~item() noexcept;
};

struct ircd::m::sync::data
:instance_list<ircd::m::sync::data>
{
	/// Range to synchronize. Starting index is inclusive, ending index is
	/// exclusive. Generally the starting index is a since token, and ending
	/// index is one beyond the vm::current_sequence and used for next_batch.
	m::events::range range;

	/// Whether to enable phased sync mode. The range.first will be <= 0
	/// in this case, and only handlers with the phased feature
	bool phased {false};

	/// Prefetch mode. Supporting item handlers will initiate prefetches for
	/// their data without writing to output.
	bool prefetch {false};

	/// Statistics tracking. If null, stats won't be accumulated for the sync.
	sync::stats *stats {nullptr};

	/// The client. This may be null if sync is being called internally.
	ircd::client *client {nullptr};
	const sync::args *args {nullptr};

	// User related
	const m::user user;
	const m::user::room user_room;
	const m::room::state user_state;
	const m::user::rooms user_rooms;
	const std::string filter_buf;
	const m::filter filter;
	const device::id device_id;

	/// The json::stack master object
	json::stack *out {nullptr};

	// apropos contextual
	const m::event *event {nullptr};
	const m::room *room {nullptr};
	string_view membership;
	int64_t room_depth {0}; // if *room
	event::idx room_head {0}; // if *room
	event::idx event_idx {0}; // if *event
	string_view client_txnid;

	data(const m::user &user,
	     const m::events::range &range,
	     ircd::client *const &client = nullptr,
	     json::stack *const &out = nullptr,
	     sync::stats *const &stats = nullptr,
	     const sync::args *const &args = nullptr,
	     const device::id &device_id = {});

	data(data &&) = delete;
	data(const data &) = delete;
	~data() noexcept;
};

struct ircd::m::sync::stats
{
	ircd::timer timer;
	size_t flush_bytes {0};
	size_t flush_count {0};
};

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

	/// Parse the since token string; this may be two numbers separated by '_'
	/// or it may be one number, or none. defaults to '0' for initial_sync.
	/// The second number is used as a next_batch value cookie we gave to
	/// the client (used during phased polylog sync)
	std::pair<string_view, string_view> since_token;

	/// 6.2.1 A point in time to continue a sync from.
	uint64_t since;

	/// This is the raw (non-spec) next_batch token which can be supplied by
	/// the client as an upper-bound on the window of this sync operation.
	/// If this is non-empty, the value takes precedence and will be strictly
	/// adhered to. Otherwise, the next_batch below may be computed by the
	/// server and may be violated on longpolls.
	string_view next_batch_token;

	/// This is named the same as the next_batch response value passed to the
	/// client at the conclusion of the sync operation because it will literally
	/// pass through this value. The next sync operation will then start at this
	/// value. This token is an event_idx, like the since token. Note it may point
	/// to an event that does not yet exist past-the-end.
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

inline bool
ircd::m::sync::apropos(const data &d,
                       const event &event)
{
	return apropos(d, index(std::nothrow, event));
}

inline bool
ircd::m::sync::apropos(const data &d,
                       const event::id &event_id)
{
	return apropos(d, index(std::nothrow, event_id));
}

inline bool
ircd::m::sync::apropos(const data &d,
                       const event::idx &event_idx)
{
	return d.phased ||
	(event_idx >= d.range.first && event_idx < d.range.second);
}
