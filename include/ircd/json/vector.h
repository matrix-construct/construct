/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#define HAVE_IRCD_JSON_VECTOR_H

namespace ircd::json
{
	struct vector;
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
	size_t count() const;
	size_t size() const;

	value_type at(const size_t &i) const;
	value_type operator[](const size_t &i) const;

	using string_view::string_view;
};

struct ircd::json::vector::const_iterator
{
	using value_type = const object;
	using pointer = value_type *;
	using reference = value_type &;
	using iterator = const_iterator;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using iterator_category = std::forward_iterator_tag;

  protected:
	friend class vector;

	const char *start;
	const char *stop;
	object state;

	const_iterator(const char *const &start, const char *const &stop)
	:start{start}
	,stop{stop}
	{}

  public:
	value_type *operator->() const               { return &state;                                  }
	value_type &operator*() const                { return *operator->();                           }

	const_iterator &operator++();

	friend bool operator==(const const_iterator &, const const_iterator &);
	friend bool operator!=(const const_iterator &, const const_iterator &);
	friend bool operator<=(const const_iterator &, const const_iterator &);
	friend bool operator>=(const const_iterator &, const const_iterator &);
	friend bool operator<(const const_iterator &, const const_iterator &);
	friend bool operator>(const const_iterator &, const const_iterator &);
};

inline ircd::json::vector::value_type
ircd::json::vector::operator[](const size_t &i)
const
{
	const auto it(find(i));
	return it != end()? *it : object{};
}

inline ircd::json::vector::value_type
ircd::json::vector::at(const size_t &i)
const
{
	const auto it(find(i));
	return likely(it != end())? *it : throw not_found("indice %zu", i);
}

inline ircd::json::vector::const_iterator
ircd::json::vector::find(size_t i)
const
{
	auto it(begin());
	for(; it != end() && i; ++it, i--);
	return it;
}

__attribute__((warning("Taking string_view::size() not the count() of vector elements")))
inline size_t
ircd::json::vector::size()
const
{
	return count();
}

inline size_t
ircd::json::vector::count()
const
{
	return std::distance(begin(), end());
}

inline bool
ircd::json::operator==(const vector::const_iterator &a, const vector::const_iterator &b)
{
	return a.start == b.start;
}

inline bool
ircd::json::operator!=(const vector::const_iterator &a, const vector::const_iterator &b)
{
	return a.start != b.start;
}

inline bool
ircd::json::operator<=(const vector::const_iterator &a, const vector::const_iterator &b)
{
	return a.start <= b.start;
}

inline bool
ircd::json::operator>=(const vector::const_iterator &a, const vector::const_iterator &b)
{
	return a.start >= b.start;
}

inline bool
ircd::json::operator<(const vector::const_iterator &a, const vector::const_iterator &b)
{
	return a.start < b.start;
}

inline bool
ircd::json::operator>(const vector::const_iterator &a, const vector::const_iterator &b)
{
	return a.start > b.start;
}
