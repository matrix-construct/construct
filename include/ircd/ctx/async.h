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
#define HAVE_IRCD_CTX_ASYNC_H

namespace ircd {
namespace ctx  {

template<class F,
         class... A>
constexpr bool
is_void_result()
{
	return std::is_void<typename std::result_of<F (A...)>::type>::value;
}

template<class F,
         class... A>
using future_void = typename std::enable_if<is_void_result<F, A...>(), future<void>>::type;

template<class F,
         class... A>
using future_value = typename std::enable_if<!is_void_result<F, A...>(),
                     future<typename std::result_of<F (A...)>::type>>::type;

template<size_t stack_size = DEFAULT_STACK_SIZE,
         context::flags flags = (context::flags)0,
         class F,
         class... A>
future_value<F, A...>
async(F&& f,
      A&&... a)
{
	using R = typename std::result_of<F (A...)>::type;

	auto func(std::bind(std::forward<F>(f), std::forward<A>(a)...));
	auto p(std::make_shared<promise<R>>());
	auto wrapper([p, func(std::move(func))]
	() mutable -> void
	{
		p->set_value(func());
	});

	future<R> ret(*p);
	//TODO: DEFER_POST?
	context(stack_size, std::move(wrapper), context::DETACH | context::POST | flags);
	return ret;
}

template<size_t stack_size = DEFAULT_STACK_SIZE,
         context::flags flags = context::flags(0),
         class F,
         class... A>
future_void<F, A...>
async(F&& f,
      A&&... a)
{
	using R = typename std::result_of<F (A...)>::type;

	auto func(std::bind(std::forward<F>(f), std::forward<A>(a)...));
	auto p(std::make_shared<promise<R>>());
	auto wrapper([p, func(std::move(func))]
	() mutable -> void
	{
		func();
		p->set_value();
	});

	future<R> ret(*p);
	//TODO: DEFER_POST?
	context(stack_size, std::move(wrapper), context::DETACH | context::POST | flags);
	return ret;
}

} // namespace ctx
} // namespace ircd
