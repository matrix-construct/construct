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
	using less_function = bool (const string_view &, const string_view &);
	using equal_function = bool (const string_view &, const string_view &);
	using separator_function = void (std::string &, const string_view &);
	using successor_function = void (std::string &);

	string_view name;
	less_function *less {nullptr};
	equal_function *equal {nullptr};
	std::function<separator_function> separator;
	std::function<successor_function> successor;
	bool hashable {true};
};

struct ircd::db::cmp_string_view
:db::comparator
{
	static bool less(const string_view &a, const string_view &b) noexcept
	{
		return a < b;
	}

	static bool equal(const string_view &a, const string_view &b) noexcept
	{
		return a == b;
	}

	cmp_string_view();
};

struct ircd::db::reverse_cmp_string_view
:db::comparator
{
	static bool less(const string_view &a, const string_view &b) noexcept;

	static bool equal(const string_view &a, const string_view &b) noexcept
	{
		return a == b;
	}

	reverse_cmp_string_view();
};

template<class T>
struct ircd::db::cmp_integer
:db::comparator
{
	static bool less(const string_view &sa, const string_view &sb) noexcept
	{
		const byte_view<T> a{sa}, b{sb};
		return a < b;
	}

	static bool equal(const string_view &sa, const string_view &sb) noexcept
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
	static bool less(const string_view &sa, const string_view &sb) noexcept
	{
		const byte_view<T> a{sa}, b{sb};
		return a > b;
	}

	static bool equal(const string_view &sa, const string_view &sb) noexcept
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
