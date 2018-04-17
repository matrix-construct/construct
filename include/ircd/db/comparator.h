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
#define HAVE_IRCD_DB_COMPARATOR_H

namespace ircd::db
{
	struct comparator;

	template<class T> struct cmp_integer;
	template<class T> struct reverse_cmp_integer;

	struct cmp_int64_t;
	struct reverse_cmp_int64_t;

	struct cmp_uint64_t;
	struct reverse_cmp_uint64_t;

	struct cmp_string_view;
	struct reverse_cmp_string_view;
}

struct ircd::db::comparator
{
	string_view name;
	std::function<bool (const string_view &, const string_view &)> less;
	std::function<bool (const string_view &, const string_view &)> equal;
};

struct ircd::db::cmp_string_view
:db::comparator
{
	static bool less(const string_view &a, const string_view &b)
	{
		return a < b;
	}

	static bool equal(const string_view &a, const string_view &b)
	{
		return a == b;
	}

	cmp_string_view()
	:db::comparator{"string_view", &less, &equal}
	{}
};

struct ircd::db::reverse_cmp_string_view
:db::comparator
{
	static bool less(const string_view &a, const string_view &b)
	{
		/// RocksDB sez things will not work correctly unless a shorter string
		/// result returns less than a longer string even if one intends some
		/// reverse ordering
		if(a.size() < b.size())
			return true;

		/// Furthermore, b.size() < a.size() returning false from this function
		/// appears to not be correct. The reversal also has to also come in
		/// the form of a bytewise forward iteration.
		return std::memcmp(a.data(), b.data(), std::min(a.size(), b.size())) > 0;
	}

	static bool equal(const string_view &a, const string_view &b)
	{
		return a == b;
	}

	reverse_cmp_string_view()
	:db::comparator{"reverse_string_view", &less, &equal}
	{}
};

template<class T>
struct ircd::db::cmp_integer
:db::comparator
{
	static bool less(const string_view &sa, const string_view &sb)
	{
		const byte_view<T> a{sa}, b{sb};
		return a < b;
	}

	static bool equal(const string_view &sa, const string_view &sb)
	{
		const byte_view<T> a{sa}, b{sb};
		return a == b;
	}

	cmp_integer()
	:db::comparator{"integer", &less, &equal}
	{}
};

template<class T>
struct ircd::db::reverse_cmp_integer
:db::comparator
{
	static bool less(const string_view &a, const string_view &b)
	{
		return !cmp_integer<T>::less(a, b);
	}

	static bool equal(const string_view &a, const string_view &b)
	{
		return cmp_integer<T>::equal(a, b);
	}

	reverse_cmp_integer()
	:db::comparator{"reverse_integer", &less, &equal}
	{}
};

struct ircd::db::cmp_int64_t
:cmp_integer<int64_t>
{
	using cmp_integer<int64_t>::cmp_integer;
};

struct ircd::db::reverse_cmp_int64_t
:reverse_cmp_integer<int64_t>
{
	using reverse_cmp_integer<int64_t>::reverse_cmp_integer;
};

struct ircd::db::cmp_uint64_t
:cmp_integer<uint64_t>
{
	using cmp_integer<uint64_t>::cmp_integer;
};

struct ircd::db::reverse_cmp_uint64_t
:reverse_cmp_integer<uint64_t>
{
	using reverse_cmp_integer<uint64_t>::reverse_cmp_integer;
};
