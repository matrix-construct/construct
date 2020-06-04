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
#define HAVE_IRCD_M_SYNC_DATA_H

namespace ircd
{
	struct client;
}

namespace ircd::m::sync
{
	struct data;

	string_view loghead(const data &);

	bool apropos(const data &, const event::idx &);
	bool apropos(const data &, const event::id &);
	bool apropos(const data &, const event &);
}

/// sync::data is the primary argument structure passed around to each
/// sync::item handler to effect a /sync. This contains everything the item
/// needs to provide response content.
///
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

	/// Parsed arguments for the request.
	const sync::args *args {nullptr};

	/// User apropos.
	const m::user user;

	/// User's room convenience.
	const m::user::room user_room;

	/// User's room state convenience.
	const m::room::state user_state;

	/// User's rooms interface convenience.
	const m::user::rooms user_rooms;

	/// Buffer for supplied or fetched filter.
	const std::string filter_buf;

	/// Structured parse of the above filter.
	const m::filter filter;

	/// User's device ID from the access token.
	const device::id device_id;

	/// Event apropos (may be null for polylog).
	const m::event *event {nullptr};

	/// Event room's interface convenience.
	const m::room *room {nullptr};

	/// User's membership in event's room.
	string_view membership;

	/// Event's depth in room.
	int64_t room_depth {0}; // if *room

	/// Event room's top-head sequence number
	event::idx room_head {0}; // if *room

	/// Event's sequence number
	event::idx event_idx {0}; // if *event

	/// Client transaction_id apropos
	string_view client_txnid;

	/// The json::stack master object
	json::stack *out {nullptr};

	/// Set by a linear sync handler; indicates the handler cannot fulfill
	/// the request because the polylog sync handler should be used instead;
	bool reflow_full_state {false};

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
