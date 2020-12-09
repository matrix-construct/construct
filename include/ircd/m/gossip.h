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
#define HAVE_IRCD_M_GOSSIP_H

namespace ircd::m
{
	struct gossip;
}

/// Matrix Gossip is a mechanism that proactively resolves the head (forward-
/// extremities) of a room on remote servers by sending the events they are
/// missing if we have them. Gossip may be performed multiple times, checking
/// for changes in the remote head and repeating based on options or until
/// completion.
struct ircd::m::gossip
:instance_list<ircd::m::gossip>
{
	struct opts;
	struct result;

	static log::log log;

	const struct opts &opts;
	std::list<result> requests;
	std::set<uint128_t> attempts;

  private:
	bool full() const noexcept;
	bool started(const event::id &, const string_view &) const;

	bool handle(result &);
	bool handle();
	bool start(const event::id &, const string_view &);
	bool submit(const event::id &, const string_view &);
	bool handle_head(const m::event &);
	bool gossip_head();

  public:
	gossip(const struct opts &);
	gossip(const gossip &) = delete;
	gossip &operator=(const gossip &) = delete;
	~gossip() noexcept;
};

struct ircd::m::gossip::opts
{
	/// Room apropos; when room.event_id is true, only that event will be
	/// the subject of gossip and that is only if the remote's head requires
	/// it. room.event_id should not be given in most cases.
	m::room room;

	/// When hint_only=true this string is used to conduct gossip with the
	/// single remote given.
	string_view hint;

	/// Forces remote operations to the remote given in the hint only.
	bool hint_only {false};

	/// Depthwise window of gossip: no gossip for events outside of a given
	/// depth window. Ignored if !depth.second.
	pair<int64_t> depth {0, 0};

	/// Indexwise window of gossip: no gossip for events with a value outside
	/// of the window.
	pair<event::idx> ref {0, -1UL};

	/// The number of rounds the algorithm runs for.
	size_t rounds {-1UL};

	/// Total event limit over all operations.
	size_t max {-1UL};

	/// Limit the number of gossips in flight at any given time.
	size_t width {128};
};

struct ircd::m::gossip::result
{
	unique_mutable_buffer buf;
	string_view txn;
	string_view txnid;
	string_view remote;
	event::id event_id;
	m::fed::send request;
};
