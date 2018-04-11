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
#define HAVE_IRCD_CTX_WHEN_H

namespace ircd::ctx
{
	template<class it> future<it> when_any(it first, const it &last);
	template<class it> future<void> when_all(it first, const it &last);
}

/// Returns a future which becomes ready when all of the futures in the
/// collection become ready. This future has a void payload to minimize
/// its cost since this indication is positively unate.
template<class it>
ircd::ctx::future<void>
ircd::ctx::when_all(it first,
                    const it &last)
{
	static const auto then
	{
		[](promise<void> &p)
		{
			if(!p.valid())
				return;

			if(refcount(p.state()) < 2)
				return p.set_value();

			return remove(p.state(), p);
		}
	};

	promise<void> p;
	const auto set_then
	{
		[&p](it &f)
		{
			f->state().then = [p]
			(shared_state_base &) mutable
			{
				then(p);
			};
		}
	};

	future<void> ret(p);
	for(; first != last; ++first)
		if(is(first->state(), future_state::PENDING))
			set_then(first);

	if(refcount(p.state()) <= 1)
		p.set_value();

	return ret;
}

/// Returns a future which becomes ready when any of the futures in the
/// iteration become ready or are already ready. The future that when_any()
/// eventually indicates is then considered "observed" which means you
/// are required to do nothing when including it in the next invocation of
/// when_any() and it won't be considered ready or pending again and the
/// collection does not have to be modified in any way.
///
/// The returned future's payload is an iterator into the collection as if
/// it were the result of an std::find() etc; thus to know its index an
/// std::distance is often satisfactory.
template<class it>
ircd::ctx::future<it>
ircd::ctx::when_any(it first,
                    const it &last)
{
	static const auto then
	{
		[](promise<it> &p, it &f)
		{
			if(!p.valid())
				return;

			set(f->state(), future_state::OBSERVED);
			p.set_value(f);
		}
	};

	promise<it> p;
	const auto set_then
	{
		[&p](it &f)
		{
			f->state().then = [p, f]             // alloc
			(shared_state_base &) mutable
			{
				then(p, f);
			};
		}
	};

	future<it> ret(p);
	for(auto f(first); f != last; ++f)
		if(is(f->state(), future_state::READY))
		{
			set(f->state(), future_state::OBSERVED);
			p.set_value(f);
			return ret;
		}

	for(; first != last; ++first)
		if(is(first->state(), future_state::PENDING))
			set_then(first);

	if(refcount(p.state()) <= 1)
		p.set_value(first);

	return ret;
}
