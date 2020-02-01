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
#define HAVE_IRCD_UTIL_MAYBE_H

namespace ircd { inline namespace util
{
	using maybe_void_type = std::tuple<bool, std::exception_ptr>;

	template<class function>
	using maybe_retval = typename std::invoke_result<function>::type;

	template<class function>
	using maybe_type = std::tuple<maybe_retval<function>, std::exception_ptr>;

	template<class function>
	constexpr bool maybe_void();

	template<class function>
	typename std::enable_if<!maybe_void<function>(), maybe_type<function>>::type
	maybe(function&& f)
	noexcept try
	{
		return
		{
			f(), std::exception_ptr{}
		};
	}
	catch(...)
	{
		return
		{
			{}, std::current_exception()
		};
	}

	template<class function>
	typename std::enable_if<maybe_void<function>(), maybe_void_type>::type
	maybe(function&& f)
	noexcept try
	{
		f();
		return
		{
			true, std::exception_ptr{}
		};
	}
	catch(...)
	{
		return
		{
			false, std::current_exception()
		};
	}

	template<class function>
	constexpr bool
	maybe_void()
	{
		return std::is_same<maybe_retval<function>, void>();
	}
}}
