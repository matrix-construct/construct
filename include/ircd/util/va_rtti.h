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
#define HAVE_IRCD_UTIL_VA_RTTI_H

namespace ircd {
inline namespace util
{
	struct va_rtti;

	const size_t VA_RTTI_MAX_SIZE
	{
		12
	};
}}

//
// Similar to a va_list, but conveying std-c++ type data acquired from a variadic template
// parameter pack acting as the ...) elipsis. This is used to implement fmt::snprintf(),
// exceptions and logger et al in their respective translation units rather than the header
// files.
//
// Limitations: The choice of a fixed array of N is because std::initializer_list doesn't
// work here and other containers may be heavy in this context. Ideas to improve this are
// welcome.
//
struct ircd::util::va_rtti
:std::array<std::tuple<const void *, const std::type_info *>, ircd::util::VA_RTTI_MAX_SIZE>
{
	using base_type = std::array<value_type, ircd::util::VA_RTTI_MAX_SIZE>;

	static constexpr size_t max_size()
	{
		return std::tuple_size<base_type>();
	}

	size_t argc;
	const size_t &size() const
	{
		return argc;
	}

	template<class... Args>
	va_rtti(Args&&... args)
	:base_type
	{{
		std::make_tuple(std::addressof(args), std::addressof(typeid(Args)))...
	}}
	,argc
	{
		sizeof...(args)
	}
	{
		assert(argc <= max_size());
		static_assert
		(
			sizeof...(args) <= max_size(),
			"Too many arguments to va_rtti"
		);
	}
};

static_assert
(
	sizeof(ircd::util::va_rtti) == (ircd::util::va_rtti::max_size() * 16) + 8,
	"va_rtti should be (8 + 8) * N + 8;"
	" where 8 + 8 are the two pointers carrying the argument and its type data;"
	" where N is the max arguments;"
	" where the final + 8 bytes holds the actual number of arguments passed;"
);
