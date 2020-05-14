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
#define HAVE_IRCD_JSON_VECTOR_ITERATOR_H

namespace ircd::json
{
	bool operator==(const vector::const_iterator &, const vector::const_iterator &);
	bool operator!=(const vector::const_iterator &, const vector::const_iterator &);
	bool operator<=(const vector::const_iterator &, const vector::const_iterator &);
	bool operator>=(const vector::const_iterator &, const vector::const_iterator &);
	bool operator<(const vector::const_iterator &, const vector::const_iterator &);
	bool operator>(const vector::const_iterator &, const vector::const_iterator &);
}

struct ircd::json::vector::const_iterator
{
	friend class vector;

	using value_type = const object;
	using pointer = value_type *;
	using reference = value_type &;
	using iterator = const_iterator;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using iterator_category = std::forward_iterator_tag;

	const char *start {nullptr};
	const char *stop {nullptr};
	object state;

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

inline ircd::json::vector::const_iterator::value_type &
ircd::json::vector::const_iterator::operator*()
const
{
	return *operator->();
}

inline ircd::json::vector::const_iterator::value_type *
ircd::json::vector::const_iterator::operator->()
const
{
	return &state;
}

inline bool
ircd::json::operator==(const vector::const_iterator &a,
                       const vector::const_iterator &b)
{
	return a.start == b.start && a.state == b.state;
}

inline bool
ircd::json::operator!=(const vector::const_iterator &a,
                       const vector::const_iterator &b)
{
	return a.start != b.start || a.state != b.state;
}

inline bool
ircd::json::operator<=(const vector::const_iterator &a,
                       const vector::const_iterator &b)
{
	return a.start <= b.start && a.state == b.state;
}

inline bool
ircd::json::operator>=(const vector::const_iterator &a,
                       const vector::const_iterator &b)
{
	return a.start >= b.start && a.state == b.state;
}

inline bool
ircd::json::operator<(const vector::const_iterator &a,
                      const vector::const_iterator &b)
{
	return a.start < b.start && a.state == b.state;
}

inline bool
ircd::json::operator>(const vector::const_iterator &a,
                      const vector::const_iterator &b)
{
	return a.start > b.start && a.state == b.state;
}
