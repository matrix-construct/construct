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
	struct request;
	using closure = std::function<bool (request &)>;

	ctx::dock dock;
	std::deque<request> queue;
	ctx::context context;
	size_t handles {0};
	size_t request_workers {0};
	size_t request_counter {0};
	size_t handles_counter {0};
	size_t fetched_counter {0};
	size_t cancels_counter {0};

	size_t wait_pending();
	void request_handle(request &);
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
	std::string key;
	database *d {nullptr};
	steady_point start;
	uint32_t cid {0};
};
