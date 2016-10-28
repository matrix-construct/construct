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
#define HAVE_IRCD_JS_GENERATOR_H

namespace ircd {
namespace js   {

struct generator
{
	heap_object state;
	heap_object last;

	bool done() const;
	template<class... args> value next(args&&...);
	template<class... args> value _throw(args&&...);
	template<class... args> value _return(args&&...);

	generator(object::handle state);
	generator(object state);
	generator() = default;
};

inline
generator::generator(object state)
:state{state}
{
}

inline
generator::generator(object::handle state)
:state{state}
{
}

template<class... args>
value
generator::_return(args&&... a)
{
	function func(get(state, "return"));
	last = func(state, std::forward<args>(a)...);
	return has(last, "value")? get(last, "value") : value{};
}

template<class... args>
value
generator::_throw(args&&... a)
{
	function func(get(state, "throw"));
	last = func(state, std::forward<args>(a)...);
	return has(last, "value")? get(last, "value") : value{};
}

template<class... args>
value
generator::next(args&&... a)
{
	function func(get(state, "next"));
	last = func(state, std::forward<args>(a)...);
	return has(last, "value")? get(last, "value") : value{};
}

inline bool
generator::done()
const
{
	return bool(get(last, "done"));
}

} // namespace js
} // namespace ircd
