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

	/// Backreference to database
	database &d;

	/// Convenience alias of the number of pools
	static constexpr const size_t POOLS
	{
		rocksdb::Env::Priority::TOTAL
	};

	static conf::item<size_t> pool_stack_size;

	std::array<std::deque<task>, POOLS> tasks;
	std::array<ctx::pool, POOLS> pool;

	state(database *const &d);
	~state() noexcept;
};

struct ircd::db::database::env::state::task
{
	void (*func)(void *arg);
	void (*cancel)(void *arg);
	void *arg;
};
