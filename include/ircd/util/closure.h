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
#define HAVE_IRCD_UTIL_CLOSURE_H

namespace ircd {
inline namespace util
{
	template<template<class, class...>
	         class F,
	         class R,
	         class... A>
	struct closure;

	template<template<class, class...>
	         class F,
	         class... A>
	struct closure<F, bool, A...>;

	/// Template for creating callback-based iterations allowing closures of
	/// either `void (...)` or `bool (...)` to be passed to the same overload.
	///
	/// This reduces/deduplicates interfaces which offer an overload for each
	/// and thus two library symbols, usually just calling each other...
	///
	template<template<class, class...>
	         class F,
	         class... A>
	using closure_bool = closure<F, bool, A...>;
}}

template<template<class, class...>
         class F,
         class R,
         class... A>
struct ircd::util::closure
:F<R (A...)>
{
	using proto_type = R (A...);
	using func_type = F<proto_type>;

	using func_type::func_type;
	using func_type::operator=;
};

template<template<class, class...>
         class F,
         class... A>
struct ircd::util::closure<F, bool, A...>
:F<bool (A...)>
{
	using proto_type = bool (A...);
	using func_type = F<proto_type>;

	template<class lambda>
	closure(lambda &&o) noexcept;
};

template<template<class, class...>
         class F,
         class... A>
template<class lambda>
[[gnu::always_inline]]
inline
ircd::util::closure<F, bool, A...>::closure(lambda &&o)
noexcept
:F<bool (A...)>
{
	[o(std::move(o))](A&&... a)
	{
		o(std::forward<A>(a)...);
		return true;
	}
}
{}
