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
#define HAVE_IRCD_DB_DATABASE_ENV_STATE_H

struct ircd::db::database::env::state
{
	struct task;
	struct pool;

	static constexpr const size_t POOLS
	{
		rocksdb::Env::Priority::TOTAL
	};

	database &d;
	std::array<std::unique_ptr<pool>, POOLS> pool;

	state(database *const &);
	~state() noexcept;
};

struct ircd::db::database::env::state::pool
{
	using Priority = rocksdb::Env::Priority;

	static conf::item<size_t> stack_size;

	database &d;
	Priority pri;
	std::deque<task> tasks;
	ctx::pool p;

	size_t cancel(void *const &tag);
	void operator()(task &&);

	void wait();
	void join();

	pool(database &, const Priority &);
	~pool() noexcept;
};

struct ircd::db::database::env::state::task
{
	void (*func)(void *arg);
	void (*cancel)(void *arg);
	void *arg;
};
