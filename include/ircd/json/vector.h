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
#define HAVE_IRCD_JSON_VECTOR_H

namespace ircd::json
{
	struct vector;

	bool empty(const json::vector &);
	size_t size(const json::vector &);
	bool operator!(const json::vector &);
}

/// Interface for non-standard, non-delimited concatenations of objects
///
/// This class parses a "vector of objects" with a similar strategy and
/// interface to that of json::array etc. The elements of the vector are
/// JSON objects delimited only by their structure. This is primarily for
/// test vectors and internal use and should not be used in public facing
/// code.
///
/// As with json::array, the same stateless forward-iteration notice applies
/// here. This object will not perform well for random access.
///
struct ircd::json::vector
:string_view
{
	struct const_iterator;

	using value_type = const object;
	using pointer = value_type *;
	using reference = value_type &;
	using iterator = const_iterator;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	const_iterator end() const;
	const_iterator begin() const;

	const_iterator find(size_t i) const;
	value_type at(const size_t &i) const;
	value_type operator[](const size_t &i) const;

	bool empty() const;
	operator bool() const;
	size_t count() const;
	size_t size() const;

	using string_view::string_view;
};

#include "vector_iterator.h"

inline ircd::json::vector::const_iterator
ircd::json::vector::end()
const
{
	return { string_view::end(), string_view::end() };
}
