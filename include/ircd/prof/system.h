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
#define HAVE_IRCD_PROF_SYSTEM_H

namespace ircd::prof
{
	struct system;

	using read_closure = std::function<void (const type &, const uint64_t &val)>;
	void for_each(const const_buffer &read, const read_closure &);

	system &hotsample(system &) noexcept;
	system &operator+=(system &a, const system &b);
	system &operator-=(system &a, const system &b);
	system operator+(const system &a, const system &b);
	system operator-(const system &a, const system &b);
}

/// Frontend to perf_event_open(2). This has the highest resolution.
struct ircd::prof::system
:std::array<std::array<uint64_t, 2>, 7>
{
	using array_type = std::array<std::array<uint64_t, 2>, 7>;

	static prof::group group;

	// [N][0] = KERNEL, [N][1] = USER
	//
	// 0: TIME_PROF,
	// 1: TIME_CPU,
	// 2: TIME_TASK,
	// 3: PF_MINOR,
	// 4: PF_MAJOR,
	// 5: SWITCH_TASK,
	// 6: SWITCH_CPU,

	system(sample_t) noexcept;
	system() :array_type{{{0}}} {}
	~system() noexcept;
};
