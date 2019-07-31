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
#define HAVE_IRCD_PROF_RESOURCE_H

namespace ircd::prof
{
	struct resource;

	resource &operator+=(resource &a, const resource &b);
	resource &operator-=(resource &a, const resource &b);
	resource operator+(const resource &a, const resource &b);
	resource operator-(const resource &a, const resource &b);
}

/// Frontend to getrusage(2). This has higher resolution than prof::times
/// in practice with slight added expense.
struct ircd::prof::resource
:std::array<uint64_t, 9>
{
	enum
	{
		TIME_USER, // microseconds
		TIME_KERN, // microseconds
		RSS_MAX,
		PF_MINOR,
		PF_MAJOR,
		BLOCK_IN,
		BLOCK_OUT,
		SCHED_YIELD,
		SCHED_PREEMPT,
	};

	resource(sample_t);
	resource()
	:std::array<uint64_t, 9>{{0}}
	{}
};
