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
#define HAVE_IRCD_JSON_ARRAY_H

namespace ircd::json
{
	struct array;

	string_view stringify(mutable_buffer &buf, const string_view *const &begin, const string_view *const &end);
	string_view stringify(mutable_buffer &buf, const std::string *const &begin, const std::string *const &end);

	size_t serialized(const string_view *const &begin, const string_view *const &end);
	size_t serialized(const std::string *const &begin, const std::string *const &end);
}

/// Lightweight interface to a JSON array string.
///
/// This object accepts queries with numerical indexing. The same parsing
/// approach is used in ircd::json::object and that is important to note here:
/// iterating this array by incrementing your own numerical index and making
/// calls into this object is NOT efficient. Simply put, do not do something
/// like `for(int x=0; x<array.count(); x++) array.at(x)` as that will parse
/// the array from the beginning on every single increment. Instead, use the
/// provided iterator object.
///
struct ircd::json::array
:string_view
{
	struct const_iterator;

	using value_type = const string_view;
	using pointer = value_type *;
	using reference = value_type &;
	using iterator = const_iterator;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	const_iterator end() const;
	const_iterator begin() const;

	const_iterator find(size_t i) const;
	size_t count() const;

	template<class T> T at(const size_t &i) const;
	string_view at(const size_t &i) const;
	string_view operator[](const size_t &i) const;

	explicit operator std::string() const;

	using string_view::string_view;

	template<class it> static string_view stringify(mutable_buffer &, const it &b, const it &e);
	friend string_view stringify(mutable_buffer &, const array &);
	friend std::ostream &operator<<(std::ostream &, const array &);
};

struct ircd::json::array::const_iterator
{
	using value_type = const string_view;
	using pointer = value_type *;
	using reference = value_type &;
	using iterator = const_iterator;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using iterator_category = std::forward_iterator_tag;

  protected:
	friend class array;

	const char *start;
	const char *stop;
	string_view state;

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

inline ircd::string_view
ircd::json::array::operator[](const size_t &i)
const
{
	const auto it(find(i));
	return it != end()? *it : string_view{};
}

template<class T>
T
ircd::json::array::at(const size_t &i)
const try
{
	return lex_cast<T>(at(i));
}
catch(const bad_lex_cast &e)
{
	throw type_error("indice %zu must cast to type %s", i, typeid(T).name());
}

inline ircd::string_view
ircd::json::array::at(const size_t &i)
const
{
	const auto it(find(i));
	return likely(it != end())? *it : throw not_found("indice %zu", i);
}

inline ircd::json::array::const_iterator
ircd::json::array::find(size_t i)
const
{
	auto it(begin());
	for(; it != end() && i; ++it, i--);
	return it;
}

inline size_t
ircd::json::array::count()
const
{
	return std::distance(begin(), end());
}

inline bool
ircd::json::operator==(const array::const_iterator &a, const array::const_iterator &b)
{
	return a.start == b.start;
}

inline bool
ircd::json::operator!=(const array::const_iterator &a, const array::const_iterator &b)
{
	return a.start != b.start;
}

inline bool
ircd::json::operator<=(const array::const_iterator &a, const array::const_iterator &b)
{
	return a.start <= b.start;
}

inline bool
ircd::json::operator>=(const array::const_iterator &a, const array::const_iterator &b)
{
	return a.start >= b.start;
}

inline bool
ircd::json::operator<(const array::const_iterator &a, const array::const_iterator &b)
{
	return a.start < b.start;
}

inline bool
ircd::json::operator>(const array::const_iterator &a, const array::const_iterator &b)
{
	return a.start > b.start;
}
