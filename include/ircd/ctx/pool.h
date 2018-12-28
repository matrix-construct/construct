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
#define HAVE_IRCD_CTX_POOL_H

namespace ircd::ctx
{
	struct pool;

	const string_view &name(const pool &);
	void debug_stats(const pool &);
}

struct ircd::ctx::pool
{
	struct opts;
	using closure = std::function<void ()>;

	static const string_view default_name;
	static const opts default_opts;

	string_view name {default_name};
	const opts *opt {&default_opts};
	size_t running {0};
	size_t working {0};
	dock q_max;
	queue<closure> q;
	std::vector<context> ctxs;

	void work();
	void main() noexcept;

  public:
	explicit operator const opts &() const;

	// indicators
	auto size() const                  { return ctxs.size();                   }
	auto queued() const                { return q.size();                      }
	auto active() const                { return working;                       }
	auto avail() const                 { return running - active();            }
	auto pending() const               { return active() + queued();           }

	// dispatch to pool
	template<class F, class... A> future_void<F, A...> async(F&&, A&&...);
	template<class F, class... A> future_value<F, A...> async(F&&, A&&...);
	void operator()(closure);

	// control panel
	void add(const size_t & = 1);
	void del(const size_t & = 1);
	void set(const size_t &);
	void min(const size_t &);
	void terminate();
	void interrupt();
	void join();

	pool(const string_view &name = default_name,
	     const opts & = default_opts);

	pool(pool &&) = delete;
	pool(const pool &) = delete;
	pool &operator=(pool &&) = delete;
	pool &operator=(const pool &) = delete;
	~pool() noexcept;

	friend const string_view &name(const pool &);
	friend void debug_stats(const pool &);
};

struct ircd::ctx::pool::opts
{
	/// When the pool spawns a new context this will be the stack size it has.
	size_t stack_size { DEFAULT_STACK_SIZE };

	/// When the pool is constructed this will be how many contexts it spawns
	/// This value may be ignored for static duration instances.
	size_t initial_ctxs {0};

	/// Hard-limit for jobs queued. A submit to the pool over this limit throws
	/// an exception. Default is -1, effectively unlimited.
	ssize_t queue_max_hard {-1};

	/// Soft-limit for jobs queued. The behavior of the limit is configurable.
	/// The default is 0, meaning if there is no context available to service
	/// the request being submitted then the soft limit is immediately reached.
	/// See the specific behavior options following this.
	ssize_t queue_max_soft {0};

	/// Yield a context submitting to the pool if it will violate the soft
	/// limit. This is true by default. Note the default of 0 for the
	/// soft-limit itself combined with this: by default there is no queueing
	/// of jobs at all! This behavior purposely propagates flow control by
	/// slowing down the submitting context and prevents flooding the queue.
	/// This option has no effect if the submitter is not on any ircd::ctx.
	bool queue_max_blocking {true};

	/// Log a DWARNING (developer-warning level) when the soft limit is
	/// exceeded. The soft-limit is never actually exceeded when contexts
	/// are blocked from submitting (see: queue_max_blocking). This warning
	/// will still be seen for submissions outside any ircd::ctx.
	bool queue_max_dwarning {true};
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
	operator()([p(std::move(p)), func(std::move(func))]
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
	operator()([p(std::move(p)), func(std::move(func))]
	{
		func();
		p.set_value();
	});

	return ret;
}

inline ircd::ctx::pool::operator
const opts &()
const
{
	assert(opt);
	return *opt;
}
