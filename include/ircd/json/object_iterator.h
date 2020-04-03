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
#define HAVE_IRCD_JSON_OBJECT_ITERATOR_H

namespace ircd::json
{
	bool operator==(const object::const_iterator &, const object::const_iterator &);
	bool operator!=(const object::const_iterator &, const object::const_iterator &);
	bool operator<=(const object::const_iterator &, const object::const_iterator &);
	bool operator>=(const object::const_iterator &, const object::const_iterator &);
	bool operator<(const object::const_iterator &, const object::const_iterator &);
	bool operator>(const object::const_iterator &, const object::const_iterator &);
}

struct ircd::json::object::const_iterator
{
	friend class object;

	using key_type = string_view;
	using mapped_type = string_view;
	using value_type = const member;
	using pointer = value_type *;
	using reference = value_type &;
	using size_type = size_t;
	using difference_type = size_t;
	using key_compare = std::less<value_type>;
	using iterator_category = std::forward_iterator_tag;

	const char *start {nullptr};
	const char *stop {nullptr};
	member state;

	const_iterator(const char *const &start, const char *const &stop)
	:start{start}
	,stop{stop}
	{}

  public:
	value_type *operator->() const;
	value_type &operator*() const;

	const_iterator &operator++();

	const_iterator() = default;
};

inline ircd::json::object::const_iterator::value_type &
ircd::json::object::const_iterator::operator*()
const
{
	return *operator->();
}

inline ircd::json::object::const_iterator::value_type *
ircd::json::object::const_iterator::operator->()
const
{
	return &state;
}

inline bool
ircd::json::operator==(const object::const_iterator &a,
                       const object::const_iterator &b)
{
	return a.start == b.start;
}

inline bool
ircd::json::operator!=(const object::const_iterator &a,
                       const object::const_iterator &b)
{
	return a.start != b.start;
}

inline bool
ircd::json::operator<=(const object::const_iterator &a,
                       const object::const_iterator &b)
{
	return a.start <= b.start;
}

inline bool
ircd::json::operator>=(const object::const_iterator &a,
                       const object::const_iterator &b)
{
	return a.start >= b.start;
}

inline bool
ircd::json::operator<(const object::const_iterator &a,
                      const object::const_iterator &b)
{
	return a.start < b.start;
}

inline bool
ircd::json::operator>(const object::const_iterator &a,
                      const object::const_iterator &b)
{
	return a.start > b.start;
}
