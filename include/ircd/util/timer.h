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
#define HAVE_IRCD_UTIL_TIMER_H

namespace ircd::util
{
	struct timer;
}

struct ircd::util::timer
{
	IRCD_OVERLOAD(nostart)
	using clock = std::chrono::steady_clock;

	nanoseconds accumulator {0ns};
	clock::time_point start {clock::now()};

	bool stopped() const;
	template<class duration = std::chrono::seconds> duration get() const;
	template<class duration = std::chrono::seconds> duration at() const;
	string_view pretty(const mutable_buffer &out, const int &fmt = 0) const;
	std::string pretty(const int &fmt = 0) const;
	void cont();
	void stop();

	timer(const std::function<void ()> &);
	timer(nostart_t);
	timer() = default;
};

inline
ircd::util::timer::timer(nostart_t)
:start{clock::time_point::min()}
{
}

template<class duration>
duration
ircd::util::timer::at()
const
{
	const auto now(clock::now());
	return duration_cast<duration>(now - start);
}

template<class duration>
duration
ircd::util::timer::get()
const
{
	return std::chrono::duration_cast<duration>(accumulator);
}
