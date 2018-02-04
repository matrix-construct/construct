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

	struct cmp_int64_t;
	struct reverse_cmp_int64_t;

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

struct ircd::db::cmp_int64_t
:db::comparator
{
	static bool less(const string_view &sa, const string_view &sb)
	{
		assert(sa.size() == sizeof(int64_t) && sb.size() == sizeof(int64_t));
		const byte_view<int64_t> a{sa}, b{sb};
		return a < b;
	}

	static bool equal(const string_view &sa, const string_view &sb)
	{
		assert(sa.size() == sizeof(int64_t) && sb.size() == sizeof(int64_t));
		const byte_view<int64_t> a{sa}, b{sb};
		return a == b;
	}

	cmp_int64_t()
	:db::comparator{"int64_t", &less, &equal}
	{}
};

struct ircd::db::reverse_cmp_int64_t
:db::comparator
{
	static bool less(const string_view &a, const string_view &b)
	{
		return !cmp_int64_t::less(a, b);
	}

	static bool equal(const string_view &a, const string_view &b)
	{
		return cmp_int64_t::equal(a, b);
	}

	reverse_cmp_int64_t()
	:db::comparator{"reverse_int64_t", &less, &equal}
	{}
};
