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

	bool operator==(const member &a, const member &b);
	bool operator==(const member &a, const string_view &b);
	bool operator!=(const member &a, const member &b);
	bool operator!=(const member &a, const string_view &b);
	bool operator<(const member &a, const member &b);
	bool operator<(const member &a, const string_view &b);

	bool sorted(const member *begin, const member *end);

	size_t serialized(const member *begin, const member *end);
	size_t serialized(const members &);
	size_t serialized(const member &);

	string_view stringify(mutable_buffer &, const member *begin, const member *end);
	string_view stringify(mutable_buffer &, const members &);
	string_view stringify(mutable_buffer &, const member &);

	std::ostream &operator<<(std::ostream &, const member &);
}

/// A pair of json::value representing state for a member of an object.
///
/// This is slightly heavier than object::member as that only deals with
/// a pair of strings while the value here holds more diverse native state.
///
struct ircd::json::member
:std::pair<value, value>
{
	explicit member(const string_view &k);
	explicit member(const object::member &m);
	template<class K, class V> member(K&&, V&&);
	template<class K> member(K&&, value);
	member() = default;
};

template<class K,
         class V>
inline __attribute__((always_inline))
ircd::json::member::member(K&& k,
                           V&& v)
:std::pair<value, value>
{
	value { std::forward<K>(k), json::STRING }, value { std::forward<V>(v) }
}
{}

template<class K>
inline __attribute__((always_inline))
ircd::json::member::member(K&& k,
                           value v)
:std::pair<value, value>
{
	value { std::forward<K>(k), json::STRING }, std::move(v)
}
{}

inline
ircd::json::member::member(const object::member &m)
:member
{
	m.first, value { m.second }
}
{}

inline
ircd::json::member::member(const string_view &k)
:member
{
	k, value{}
}
{}

inline ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const members &list)
{
	return stringify(buf, std::begin(list), std::end(list));
}

inline ircd::string_view
ircd::json::stringify(mutable_buffer &buf,
                      const member &m)
{
	return stringify(buf, &m, &m + 1);
}

inline size_t
ircd::json::serialized(const members &m)
{
	return serialized(std::begin(m), std::end(m));
}

inline size_t
ircd::json::serialized(const member &member)
{
	return serialized(member.first) + 1 + serialized(member.second);
}

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
