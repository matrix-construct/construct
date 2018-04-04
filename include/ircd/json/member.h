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
#define HAVE_IRCD_JSON_MEMBER_H

namespace ircd::json
{
	struct member;

	bool sorted(const member *const &begin, const member *const &end);
	size_t serialized(const member *const &begin, const member *const &end);
	string_view stringify(mutable_buffer &, const member *const &begin, const member *const &end);
}

/// A pair of json::value representing state for a member of an object.
///
/// This is slightly heavier than object::member as that only deals with
/// a pair of strings while the value here holds more diverse native state.
///
struct ircd::json::member
:std::pair<value, value>
{
	member(const string_view &key, value &&);
	template<class V> member(const string_view &key, V&&);
	explicit member(const string_view &k);
	explicit member(const object::member &m);
	member() = default;

	friend bool operator==(const member &a, const member &b);
	friend bool operator!=(const member &a, const member &b);
	friend bool operator<(const member &a, const member &b);

	friend bool operator==(const member &a, const string_view &b);
	friend bool operator!=(const member &a, const string_view &b);
	friend bool operator<(const member &a, const string_view &b);

	friend bool defined(const member &);
	friend size_t serialized(const member &);
	friend string_view stringify(mutable_buffer &, const member &);
	friend std::ostream &operator<<(std::ostream &, const member &);
};

namespace ircd::json
{
	using members = std::initializer_list<member>;

	size_t serialized(const members &);
	string_view stringify(mutable_buffer &, const members &);
}

inline
ircd::json::member::member(const string_view &key,
                           value &&v)
:std::pair<value, value>
{
	{ key, json::STRING }, std::move(v)
}
{}

template<class V>
ircd::json::member::member(const string_view &key,
                           V&& v)
:std::pair<value, value>
{
	{ key, json::STRING }, std::forward<V>(v)
}
{}

inline
ircd::json::member::member(const object::member &m)
:std::pair<value, value>
{
	m.first, value { m.second, type(m.second) }
}
{}

inline
ircd::json::member::member(const string_view &k)
:std::pair<value, value>
{
	k, string_view{}
}
{}

inline bool
ircd::json::operator<(const member &a, const member &b)
{
	return a.first < b.first;
}

inline bool
ircd::json::operator!=(const member &a, const member &b)
{
	return a.first != b.first;
}

inline bool
ircd::json::operator==(const member &a, const member &b)
{
	return a.first == b.first;
}

inline bool
ircd::json::operator<(const member &a, const string_view &b)
{
	return string_view{a.first.string, a.first.len} < b;
}

inline bool
ircd::json::operator!=(const member &a, const string_view &b)
{
	return string_view{a.first.string, a.first.len} != b;
}

inline bool
ircd::json::operator==(const member &a, const string_view &b)
{
	return string_view{a.first.string, a.first.len} == b;
}

inline bool
ircd::json::defined(const member &a)
{
	return defined(a.second);
}
