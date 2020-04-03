// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_JSON_OBJECT_MEMBER_H

namespace ircd::json
{
	bool operator==(const object::member &, const object::member &);
	bool operator!=(const object::member &, const object::member &);
	bool operator<=(const object::member &, const object::member &);
	bool operator>=(const object::member &, const object::member &);
	bool operator<(const object::member &, const object::member &);
	bool operator>(const object::member &, const object::member &);

	bool sorted(const object::member *const &, const object::member *const &);
	size_t serialized(const object::member *const &, const object::member *const &);
	size_t serialized(const object::member &);
	string_view stringify(mutable_buffer &, const object::member *const &, const object::member *const &);
	string_view stringify(mutable_buffer &, const object::member &);
	std::ostream &operator<<(std::ostream &, const object::member &);
}

struct ircd::json::object::member
:std::pair<string_view, string_view>
{
	member(const string_view &first = {}, const string_view &second = {});
};

inline
ircd::json::object::member::member(const string_view &first,
                                   const string_view &second)
:std::pair<string_view, string_view>
{
	first, second
}
{}

inline bool
ircd::json::operator==(const object::member &a,
                       const object::member &b)
{
	return a.first == b.first;
}

inline bool
ircd::json::operator!=(const object::member &a,
                       const object::member &b)
{
	return a.first != b.first;
}

inline bool
ircd::json::operator<=(const object::member &a,
                       const object::member &b)
{
	return a.first <= b.first;
}

inline bool
ircd::json::operator>=(const object::member &a,
                       const object::member &b)
{
	return a.first >= b.first;
}

inline bool
ircd::json::operator<(const object::member &a,
                      const object::member &b)
{
	return a.first < b.first;
}

inline bool
ircd::json::operator>(const object::member &a,
                      const object::member &b)
{
	return a.first > b.first;
}
