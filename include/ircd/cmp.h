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
#define HAVE_IRCD_CMP_H

// Simple case insensitive comparison convenience utils
namespace ircd
{
	struct iless;
	struct igreater;
	struct iequals;
}

/// Case insensitive string comparison deciding if two strings are equal
struct ircd::iequals
{
	using is_transparent = std::true_type;

	bool s;

	operator const bool &() const
	{
		return s;
	}

	bool operator()(const string_view &a, const string_view &b) const;

	template<class A,
	         class B>
	iequals(A&& a, B&& b)
	:s{operator()(std::forward<A>(a), std::forward<B>(b))}
	{}

	iequals() = default;
};

inline bool
ircd::iequals::operator()(const string_view &a,
                          const string_view &b)
const
{
	return std::equal(begin(a), end(a), begin(b), end(b), []
	(const char &a, const char &b)
	{
		return tolower(a) == tolower(b);
	});
}

/// Case insensitive string comparison deciding which string compares 'less'
/// than the other.
struct ircd::iless
{
	using is_transparent = std::true_type;

	bool s;

	operator const bool &() const
	{
		return s;
	}

	bool operator()(const string_view &a, const string_view &b) const;

	template<class A,
	         class B>
	iless(A&& a, B&& b)
	:s{operator()(std::forward<A>(a), std::forward<B>(b))}
	{}

	iless() = default;
};

inline bool
ircd::iless::operator()(const string_view &a,
                        const string_view &b)
const
{
	return std::lexicographical_compare(begin(a), end(a), begin(b), end(b), []
	(const char &a, const char &b)
	{
		return tolower(a) < tolower(b);
	});
}

/// Case insensitive string comparison deciding which string compares 'greater'
/// than the other.
struct ircd::igreater
{
	using is_transparent = std::true_type;

	bool s;

	operator const bool &() const
	{
		return s;
	}

	bool operator()(const string_view &a, const string_view &b) const;

	template<class A,
	         class B>
	igreater(A&& a, B&& b)
	:s{operator()(std::forward<A>(a), std::forward<B>(b))}
	{}

	igreater() = default;
};

inline bool
ircd::igreater::operator()(const string_view &a,
                           const string_view &b)
const
{
	return std::lexicographical_compare(begin(a), end(a), begin(b), end(b), []
	(const char &a, const char &b)
	{
		return tolower(a) > tolower(b);
	});
}
