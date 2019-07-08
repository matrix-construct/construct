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
	template<class R> R query(std::nothrow_t, const event::idx &, const string_view &key, R&& def, const std::function<R (const string_view &)> &);
	template<class F> auto query(std::nothrow_t, const event::idx &, const string_view &key, F&&);
	template<class F> auto query(const event::idx &, const string_view &key, F&&);
}

/// Like m::get(), but the closure returns a value which will then be returned
/// by these functions. This elides the pattern of returning a result through
/// the lambda capture when one simply wants to condition on the fetched value
/// rather than make further use of it.
///
template<class F>
auto
ircd::m::query(const event::idx &event_idx,
               const string_view &key,
               F&& closure)
{
	using R = typename std::invoke_result<F, const string_view &>::type;

	R ret;
	m::get(event_idx, key, [&ret, &closure]
	(const string_view &value)
	{
		ret = closure(value);
	});

	return ret;
}

/// See other overload documentation first. This overload implements
/// non-throwing behavior when the event_idx/key(column) is not found by
/// calling the closure with a default-constructed string so the closure can
/// return a default value.
///
template<class F>
auto
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
		{
			ret = closure(value);
		}
	};

	if(unlikely(!m::get(std::nothrow, event_idx, key, assign)))
		assign(string_view{});

	return ret;
}

/// See other overload documentation first. This overload implements
/// non-throwing behavior when the event_idx/key(column) is not found by
/// returning a default value supplied by the caller.
///
template<class R>
R
ircd::m::query(std::nothrow_t,
               const event::idx &event_idx,
               const string_view &key,
               R&& default_,
               const std::function<R (const string_view &)> &closure)
{
	R ret{std::forward<R>(default_)};
	m::get(std::nothrow, event_idx, key, [&ret, &closure]
	(const string_view &value)
	{
		ret = closure(value);
	});

	return ret;
}
