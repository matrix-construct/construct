// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_QUERY_H

namespace ircd::m
{
	template<class R, class F> R query(std::nothrow_t, const pair<event::idx> &, const string_view &key, R&& def, F&&);
	template<class R, class F> R query(std::nothrow_t, const event::idx &, const string_view &key, R&& def, F&&);

	template<class F> auto query(std::nothrow_t, const pair<event::idx> &, const string_view &key, F&&);
	template<class F> auto query(std::nothrow_t, const event::idx &, const string_view &key, F&&);

	template<class R, class F> R query(const pair<event::idx> &, const string_view &key, R&& def, F&&);
	template<class R, class F> R query(const event::idx &, const string_view &key, R&& def, F&&);

	template<class F> auto query(const pair<event::idx> &, const string_view &key, F&&);
	template<class F> auto query(const event::idx &, const string_view &key, F&&);
}

/// Like m::get(), but the closure returns a value which will then be returned
/// by these functions. This elides the pattern of returning a result through
/// the lambda capture when one simply wants to condition on the fetched value
/// rather than make further use of it.
///
template<class F>
inline auto
ircd::m::query(const event::idx &event_idx,
               const string_view &key,
               F&& closure)
{
	using R = typename std::invoke_result<F, const string_view &>::type;

	R ret;
	m::get(event_idx, key, [&ret, &closure]
	(const string_view &value)
	noexcept(std::is_nothrow_invocable<F, decltype(value)>())
	{
		ret = closure(value);
	});

	return ret;
}

template<class F>
inline auto
ircd::m::query(const pair<event::idx> &event_idx,
               const string_view &key,
               F&& closure)
{
	using R = typename std::invoke_result<F, const string_view &>::type;

	R ret;
	const event::idx idx[2]
	{
		event_idx.first, event_idx.second
	};

	m::get(idx, key, [&ret, &closure]
	(const vector_view<const string_view> res)
	noexcept(std::is_nothrow_invocable<F, const string_view &, const string_view &>())
	{
		ret = closure(res[0], res[1]);
	});

	return ret;
}

template<class R,
         class F>
inline R
ircd::m::query(const event::idx &event_idx,
               const string_view &key,
               R&& r,
               F&& f)
{
	return query(std::nothrow, event_idx, key, std::forward<R>(r), std::forward<F>(f));
}

template<class R,
         class F>
inline R
ircd::m::query(const pair<event::idx> &event_idx,
               const string_view &key,
               R&& r,
               F&& f)
{
	return query(std::nothrow, event_idx, key, std::forward<R>(r), std::forward<F>(f));
}

/// See other overload documentation first. This overload implements
/// non-throwing behavior when the event_idx/key(column) is not found by
/// calling the closure with a default-constructed string so the closure can
/// return a default value.
///
template<class F>
inline auto
ircd::m::query(std::nothrow_t,
               const event::idx &event_idx,
               const string_view &key,
               F&& closure)
{
	using R = typename std::invoke_result<F, const string_view &>::type;

	R ret;
	const auto assign
	{
		[&ret, &closure](const string_view &value)
		noexcept(std::is_nothrow_invocable<F, decltype(value)>())
		{
			ret = closure(value);
		}
	};

	if(unlikely(!m::get(std::nothrow, event_idx, key, assign)))
		assign(string_view{});

	return ret;
}

template<class F>
inline auto
ircd::m::query(std::nothrow_t,
               const pair<event::idx> &event_idx,
               const string_view &key,
               F&& closure)
{
	using R = typename std::invoke_result<F, const string_view &, const string_view &>::type;

	R ret;
	const auto compare
	{
		[&ret, &closure](const vector_view<const string_view> res)
		noexcept(std::is_nothrow_invocable<F, const string_view &, const string_view &>())
		{
			ret = closure(res[0], res[1]);
		}
	};

	const event::idx idx[2]
	{
		event_idx.first, event_idx.second
	};

	const auto mask
	{
		m::get(std::nothrow, idx, key, compare)
	};

	const auto got
	{
		__builtin_popcountl(mask)
	};

	return ret;
}

/// See other overload documentation first. This overload implements
/// non-throwing behavior when the event_idx/key(column) is not found by
/// returning a default value supplied by the caller.
///
template<class R,
         class F>
inline R
ircd::m::query(std::nothrow_t,
               const event::idx &event_idx,
               const string_view &key,
               R&& default_,
               F&& closure)
{
	R ret
	{
		std::forward<R>(default_)
	};

	m::get(std::nothrow, event_idx, key, [&ret, &closure]
	(const string_view &value)
	noexcept(std::is_nothrow_invocable<F, decltype(value)>())
	{
		ret = closure(value);
	});

	return ret;
}

template<class R,
         class F>
inline R
ircd::m::query(std::nothrow_t,
               const pair<event::idx> &event_idx,
               const string_view &key,
               R&& default_,
               F&& closure)
{
	R ret
	{
		std::forward<R>(default_)
	};

	const event::idx idx[2]
	{
		event_idx.first, event_idx.second
	};

	m::get(std::nothrow, idx, key, [&ret, &closure]
	(const vector_view<const string_view> &res)
	noexcept(std::is_nothrow_invocable<F, const string_view &, const string_view &>())
	{
		ret = closure(res[0], res[1]);
	});

	return ret;
}
