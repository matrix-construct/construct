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

  public:
	string_view name() const;
	string_view member_name() const;

	bool poll(data &, const m::event &);
	bool linear(data &);
	bool polylog(data &);

	item(std::string name,
	     handle polylog    = {},
	     handle linear     = {});

	item(item &&) = delete;
	item(const item &) = delete;
	~item() noexcept;
};

struct ircd::m::sync::data
{
	/// Range to synchronize. Starting index is inclusive, ending index is
	/// exclusive. Generally the starting index is a since token, and ending
	/// index is one beyond the vm::current_sequence and used for next_batch.
	m::events::range range;

	/// Statistics tracking. If null, stats won't be accumulated for the sync.
	sync::stats *stats {nullptr};

	/// The client. This may be null if sync is being called internally.
	ircd::client *client {nullptr};

	// User related
	const m::user user;
	const m::user::room user_room;
	const m::room::state user_state;
	const m::user::rooms user_rooms;
	const std::string filter_buf;
	const m::filter filter;

	/// The json::stack master object
	json::stack *out {nullptr};

	// apropos contextual
	const m::event *event {nullptr};
	const m::room *room {nullptr};
	string_view membership;
	event::idx room_head {0};

	data(const m::user &user,
	     const m::events::range &range,
	     ircd::client *const &client = nullptr,
	     json::stack *const &out = nullptr,
	     sync::stats *const &stats = nullptr,
	     const string_view &filter_id = {});

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
