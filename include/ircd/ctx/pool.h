/*
 *  Copyright (C) 2016 Charybdis Development Team
 *  Copyright (C) 2016 Jason Volk <jason@zemos.net>
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
#define HAVE_IRCD_CTX_POOL_H

namespace ircd::ctx
{
	struct pool;
}

struct ircd::ctx::pool
{
	using closure = std::function<void ()>;

  private:
	const char *name;
	size_t stack_size;
	size_t running;
	size_t working;
	struct dock dock;
	std::deque<closure> queue;
	std::vector<context> ctxs;

	void next();
	void main();

  public:
	// indicators
	auto size() const                            { return ctxs.size();                             }
	auto queued() const                          { return queue.size();                            }
	auto active() const                          { return working;                                 }
	auto avail() const                           { return running - working;                       }
	auto pending() const                         { return active() + queued();                     }

	// control panel
	void add(const size_t & = 1);
	void del(const size_t & = 1);
	void interrupt();
	void join();

	// dispatch function to pool
	void operator()(closure);

	// dispatch function std async style
	template<class F, class... A> future_void<F, A...> async(F&&, A&&...);
	template<class F, class... A> future_value<F, A...> async(F&&, A&&...);

	pool(const char *const &name    = "<unnamed pool>",
	     const size_t &stack_size   = DEFAULT_STACK_SIZE,
	     const size_t &             = 0);

	pool(pool &&) = delete;
	pool(const pool &) = delete;
	pool &operator=(pool &&) = delete;
	pool &operator=(const pool &) = delete;
	~pool() noexcept;

	friend void debug_stats(const pool &);
};

template<class F,
         class... A>
ircd::ctx::future_value<F, A...>
ircd::ctx::pool::async(F&& f,
                       A&&... a)
{
	using R = typename std::result_of<F (A...)>::type;

	auto func
	{
		std::bind(std::forward<F>(f), std::forward<A>(a)...)
	};

	promise<R> p;
	future<R> ret{p};
	(*this)([p(std::move(p)), func(std::move(func))]
	() -> void
	{
		p.set_value(func());
	});

	return ret;
}

template<class F,
         class... A>
ircd::ctx::future_void<F, A...>
ircd::ctx::pool::async(F&& f,
                       A&&... a)
{
	using R = typename std::result_of<F (A...)>::type;

	auto func
	{
		std::bind(std::forward<F>(f), std::forward<A>(a)...)
	};

	promise<R> p;
	future<R> ret{p};
	(*this)([p(std::move(p)), func(std::move(func))]
	() -> void
	{
		func();
		p.set_value();
	});

	return ret;
}
