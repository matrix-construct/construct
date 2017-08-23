/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
