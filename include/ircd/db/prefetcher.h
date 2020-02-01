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
	// montonic event counts
	size_t queries {0};       ///< All incoming user requests
	size_t rejects {0};       ///< Queries which were ignored; already cached
	size_t request {0};       ///< Prefetcher requests added to the queue
	size_t directs {0};       ///< Direct dispatches to db::request pool
	size_t handles {0};       ///< Incremented before dispatch to db::request
	size_t handled {0};       ///< Incremented after dispatch to db::request
	size_t fetches {0};       ///< Incremented before actual database operation
	size_t fetched {0};       ///< Incremented after actual database operation
	size_t cancels {0};       ///< Count of canceled operations

	// throughput totals
	size_t fetched_bytes_key {0};      ///< Total bytes of key data received
	size_t fetched_bytes_val {0};      ///< Total bytes of value data received

	// from last operation only
	microseconds last_snd_req {0us};   ///< duration request was queued here
	microseconds last_req_fin {0us};   ///< duration for database operation

	// accumulated latency totals
	microseconds accum_snd_req {0us};
	microseconds accum_req_fin {0us};
};
