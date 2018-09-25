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
	std::function<void (std::string &, const string_view &)> separator;
	std::function<void (std::string &)> successor;
};

struct ircd::db::cmp_string_view
:db::comparator
{
	static bool less(const string_view &a, const string_view &b);
	static bool equal(const string_view &a, const string_view &b);

	cmp_string_view();
};

struct ircd::db::reverse_cmp_string_view
:db::comparator
{
	static bool less(const string_view &a, const string_view &b);
	static bool equal(const string_view &a, const string_view &b);

	reverse_cmp_string_view();
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
	static bool less(const string_view &sa, const string_view &sb)
	{
		const byte_view<T> a{sa}, b{sb};
		return a > b;
	}

	static bool equal(const string_view &sa, const string_view &sb)
	{
		return cmp_integer<T>::equal(sa, sb);
	}

	reverse_cmp_integer()
	:db::comparator{"reverse_integer", &less, &equal}
	{}
};

struct ircd::db::cmp_int64_t
:cmp_integer<int64_t>
{
	cmp_int64_t();
	~cmp_int64_t() noexcept;
};

struct ircd::db::reverse_cmp_int64_t
:reverse_cmp_integer<int64_t>
{
	reverse_cmp_int64_t();
	~reverse_cmp_int64_t() noexcept;
};

struct ircd::db::cmp_uint64_t
:cmp_integer<uint64_t>
{
	cmp_uint64_t();
	~cmp_uint64_t() noexcept;
};

struct ircd::db::reverse_cmp_uint64_t
:reverse_cmp_integer<uint64_t>
{
	reverse_cmp_uint64_t();
	~reverse_cmp_uint64_t() noexcept;
};
