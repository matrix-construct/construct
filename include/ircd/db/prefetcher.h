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
#define HAVE_IRCD_DB_PREFETCHER_H

namespace ircd::db
{
	struct prefetcher extern *prefetcher;
}

struct ircd::db::prefetcher
{
	struct ticker;
	struct request;
	using closure = std::function<bool (request &)>;

	ctx::dock dock;
	std::deque<request> queue;
	std::unique_ptr<ticker> ticker;
	ctx::context context;
	size_t request_workers {0};

	size_t wait_pending();
	void request_handle(request &);
	size_t request_cleanup() noexcept;
	void request_worker();
	void handle();
	void worker();

  public:
	size_t cancel(const closure &);
	size_t cancel(database &);         // Cancel all for db
	size_t cancel(column &);           // Cancel all for column

	bool operator()(column &, const string_view &key, const gopts &);

	prefetcher();
	~prefetcher() noexcept;
};

struct ircd::db::prefetcher::request
{
	using key_buf = char[208];

	database *d {nullptr};             // database instance
	uint32_t cid {0};                  // column ID
	uint32_t len {0};                  // length of key
	steady_point snd;                  // submitted by user
	steady_point req;                  // request sent to database
	steady_point fin;                  // result from database
	key_buf key alignas(16);           // key buffer

	explicit operator string_view() const noexcept;

	request(database &d, const column &c, const string_view &key) noexcept;
	request() = default;
};

static_assert
(
	sizeof(ircd::db::prefetcher::request) == 256,
	"struct ircd::db::prefetcher::request fell out of alignment"
);

struct ircd::db::prefetcher::ticker
{
	template<class T> using item = ircd::stats::item<T>;

	// montonic event counts
	item<uint64_t> queries;    ///< All incoming user requests
	item<uint64_t> rejects;    ///< Queries which were ignored; already cached
	item<uint64_t> request;    ///< Prefetcher requests added to the queue
	item<uint64_t> directs;    ///< Direct dispatches to db::request pool
	item<uint64_t> handles;    ///< Incremented before dispatch to db::request
	item<uint64_t> handled;    ///< Incremented after dispatch to db::request
	item<uint64_t> fetches;    ///< Incremented before actual database operation
	item<uint64_t> fetched;    ///< Incremented after actual database operation
	item<uint64_t> cancels;    ///< Count of canceled operations

	// throughput totals
	item<uint64_t> fetched_bytes_key;    ///< Total bytes of key data received
	item<uint64_t> fetched_bytes_val;    ///< Total bytes of value data received

	// from last operation only
	item<microseconds> last_snd_req;    ///< duration request was queued here
	item<microseconds> last_req_fin;    ///< duration for database operation

	// accumulated latency totals
	item<microseconds> accum_snd_req;
	item<microseconds> accum_req_fin;

	ticker();
};
