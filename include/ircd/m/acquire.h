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
#define HAVE_IRCD_M_ACQUIRE_H

namespace ircd::m
{
	struct acquire;
}

struct ircd::m::acquire
:instance_list<ircd::m::acquire>
{
	struct opts;
	struct result;

	static log::log log;

	const struct opts &opts;
	std::list<result> fetching;

  private:
	bool full() const noexcept;
	bool handle(result &);
	bool handle();

	bool started(const event::id &) const;
	bool start(const event::id &, const string_view &, const bool &, const size_t &);
	bool submit(const event::id &, const string_view &, const bool &, const size_t &);

	bool fetch_missing(event::idx &);
	void acquire_missing();

	bool fetch_head(const m::event &, const int64_t &);
	void acquire_head();

  public:
	acquire(const struct opts &);
	acquire(const acquire &) = delete;
	acquire &operator=(const acquire &) = delete;
	~acquire() noexcept;
};

struct ircd::m::acquire::opts
{
	/// Room apropos.
	m::room room;

	/// Optional remote host first considered as the target for operations in
	/// case caller has better information for what will most likely succeed.
	string_view hint;

	/// For diagnostic and special use; forces remote operations through the
	/// hint, and fails them if the hint is insufficient.
	bool hint_only {false};

	/// Perform head acquisition prior to depthwise operations.
	bool head {true};

	/// Perform missing acquisition.
	bool missing {true};

	/// Provide a viewport size; generally obtained from the eponymous conf
	/// item and used for initial backfill
	size_t viewport_size {0};

	/// Depthwise window of acquisition; concentrate on specific depth window
	pair<int64_t> depth {0, 0};

	/// Won't fetch missing of ref outside this range.
	pair<event::idx> ref {0, -1UL};

	/// Avoids filling gaps with a depth sounding outside of the range
	pair<size_t> gap {0, -1UL};

	/// The number of rounds the algorithm runs for.
	size_t rounds {-1UL};

	/// Total event limit over all operations.
	size_t fetch_max {-1UL};

	/// Limit the number of requests in flight at any given time.
	size_t fetch_width {128};
};

struct ircd::m::acquire::result
{
	ctx::future<fetch::result> future;
	event::id::buf event_id;
};
