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
#define HAVE_IRCD_CTX_ASYNC_H

namespace ircd::ctx
{
	template<class F,
	         class... A>
	constexpr bool is_void_result();

	template<class F,
	         class... A>
	using future_void = typename std::enable_if<is_void_result<F, A...>(), future<void>>::type;

	template<class F,
	         class... A>
	using future_value = typename std::enable_if<!is_void_result<F, A...>(),
	                     future<typename std::result_of<F (A...)>::type>>::type;

	template<size_t stack_size = 0,
	         context::flags flags = context::flags(0),
	         class F,
	         class... A>
	future_value<F, A...> async(F&& f, A&&... a);

	template<size_t stack_size = 0,
	         context::flags flags = context::flags(0),
	         class F,
	         class... A>
	future_void<F, A...> async(F&& f, A&&... a);
}

template<size_t stack_size_,
         ircd::ctx::context::flags flags,
         class F,
         class... A>
ircd::ctx::future_value<F, A...>
ircd::ctx::async(F&& f,
                 A&&... a)
{
	using R = typename std::result_of<F (A...)>::type;

	promise<R> p;
	future<R> ret{p};
	auto wrapper
	{
		[p(std::move(p)), func(std::bind(std::forward<F>(f), std::forward<A>(a)...))]
		() mutable
		{
			p.set_value(func());
		}
	};

	const auto &stack_size
	{
		stack_size_?: DEFAULT_STACK_SIZE
	};

	//TODO: DEFER_POST?
	context(stack_size, std::move(wrapper), context::DETACH | context::POST | flags);
	return ret;
}

template<size_t stack_size_,
         ircd::ctx::context::flags flags,
         class F,
         class... A>
ircd::ctx::future_void<F, A...>
ircd::ctx::async(F&& f,
                 A&&... a)
{
	using R = typename std::result_of<F (A...)>::type;

	auto func
	{
		std::bind(std::forward<F>(f), std::forward<A>(a)...)
	};

	promise<R> p;
	future<R> ret{p};
	auto wrapper
	{
		[p(std::move(p)), func(std::bind(std::forward<F>(f), std::forward<A>(a)...))]
		() mutable
		{
			func();
			p.set_value();
		}
	};

	const auto &stack_size
	{
		stack_size_?: DEFAULT_STACK_SIZE
	};

	//TODO: DEFER_POST?
	context(stack_size, std::move(wrapper), context::DETACH | context::POST | flags);
	return ret;
}

template<class F,
         class... A>
constexpr bool
ircd::ctx::is_void_result()
{
	return std::is_void<typename std::result_of<F (A...)>::type>::value;
}
