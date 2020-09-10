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

namespace ircd {
inline namespace util
{
	struct timer;
}}

/// Stopwatch w/ accumulation.
///
/// This utility can be used to gauge the duration of something. The clock
/// is started by default upon construction. Users can query the running time
/// using the at(), or using the pretty() convenience.
///
/// This timer features an accumulator state so the clock can be stopped and
/// continued, and each time the accumulator is updated. The accumulator
/// value is always added into results: when the timer is stopped, the result
/// is simply the accumulator value; when the clock is running, the result is
/// the accumulator value added to the difference of the current time and the
/// last start time.
///
/// One can instantiate the timer without starting the clock with the `nostart`
/// overload. Additionally, a lambda constructor will accumulate the time it
/// takes to execute the provided function.
struct ircd::util::timer
{
	using clock = std::chrono::steady_clock;
	IRCD_OVERLOAD(nostart)

	nanoseconds accumulator
	{
		0ns
	};

	clock::time_point start
	{
		clock::now()
	};

	// Observers
	bool stopped() const;
	template<class duration = std::chrono::seconds> duration at() const;

	// Convenience wrapper for util::pretty()
	string_view pretty(const mutable_buffer &out, const int &fmt = 0) const;
	std::string pretty(const int &fmt = 0) const;

	// Control
	void cont();
	void stop();

	timer(const std::function<void ()> &);
	timer(nostart_t);
	timer() = default;
};

inline
ircd::util::timer::timer(nostart_t)
:start
{
	clock::time_point::min()
}
{}

template<class duration>
inline duration
ircd::util::timer::at()
const
{
	const auto now
	{
		!stopped()?
			clock::now():
			start
	};

	const auto elapsed
	{
		duration_cast<decltype(accumulator)>(now - start)
	};

	const auto accumulator
	{
		this->accumulator + elapsed
	};

	return duration_cast<duration>(accumulator);
}

inline bool
ircd::util::timer::stopped()
const
{
	return start == clock::time_point::min();
}
