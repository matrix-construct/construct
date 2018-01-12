// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

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

	nanoseconds accumulator;
	clock::time_point start;

	bool stopped() const;
	template<class duration = std::chrono::seconds> duration get() const;
	template<class duration = std::chrono::seconds> duration at() const;
	void cont();
	void stop();

	timer(const std::function<void ()> &);
	timer(nostart_t);
	timer();
};


/// Default construction will start the timer.
inline
ircd::util::timer::timer()
:accumulator{0ns}
,start{clock::now()}
{
}

/// timer(timer::nostart)
inline
ircd::util::timer::timer(nostart_t)
:accumulator{0ns}
,start{clock::time_point::min()}
{
}

inline
ircd::util::timer::timer(const std::function<void ()> &func)
:timer{}
{
	func();
	stop();
}

inline void
ircd::util::timer::stop()
{
	if(stopped())
		return;

	const auto now(clock::now());
	accumulator += std::chrono::duration_cast<decltype(accumulator)>(now - start);
	start = clock::time_point::min();
}

inline void
ircd::util::timer::cont()
{
	if(!stopped())
	{
		const auto now(clock::now());
		accumulator += std::chrono::duration_cast<decltype(accumulator)>(now - start);
	}

	start = clock::now();
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

inline bool
ircd::util::timer::stopped()
const
{
	return start == clock::time_point::min();
}
