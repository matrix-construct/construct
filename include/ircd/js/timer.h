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
#define HAVE_IRCD_JS_TIMER_H

namespace ircd {
namespace js   {

struct timer
{
	using steady_clock = std::chrono::steady_clock;
	using time_point = steady_clock::time_point;
	using callback = std::function<void () noexcept>;

  private:
	struct state
	{
		uint32_t sem;
		bool running;
	};

	std::mutex mutex;
	std::condition_variable cond;
	bool finished;
	callback timeout;

	time_point started;
	std::atomic<microseconds> limit;
	std::atomic<struct state> state;

	void handle(std::unique_lock<std::mutex> &lock);
	void worker();
	std::thread thread;                          //TODO: single extern thread plz

  public:
	void set(const microseconds &limit);         // Set the time limit (only when not started)
	void set(const callback &timeout);           // Set the callback (only when not started)
	bool cancel();                               // Cancel (stop) the timer
	time_point start();                          // Start the timer

	timer(const callback &timeout);
	~timer() noexcept;
};

} // namespace js
} // namespace timer
