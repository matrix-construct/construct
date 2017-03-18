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
#define HAVE_IRCD_JSON_ARR_H

namespace ircd {
namespace json {

struct arr
:string_view
{
	struct const_iterator;

	using value_type = const string_view;
	using pointer = value_type *;
	using reference = value_type &;
	using iterator = const_iterator;
	using size_type = size_t;
	using difference_type = ptrdiff_t;

	bool contains(const string_view &) const;

	const_iterator end() const;
	const_iterator begin() const;

	size_t count() const;

	const_iterator find(size_t i) const;
	string_view at(size_t i) const;
	string_view operator[](size_t i) const;

	using string_view::string_view;

	friend arr serialize(const arr &, char *&buf, char *const &stop);
	friend size_t print(char *const &buf, const size_t &max, const arr &);
	friend std::ostream &operator<<(std::ostream &, const arr &);
};

struct arr::const_iterator
{
	using value_type = const string_view;
	using pointer = value_type *;
	using reference = value_type &;
	using difference_type = size_t;
	using iterator_category = std::forward_iterator_tag;

  protected:
	friend class arr;

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

	friend bool operator==(const arr::const_iterator &, const arr::const_iterator &);
	friend bool operator!=(const arr::const_iterator &, const arr::const_iterator &);
	friend bool operator<=(const arr::const_iterator &, const arr::const_iterator &);
	friend bool operator>=(const arr::const_iterator &, const arr::const_iterator &);
	friend bool operator<(const arr::const_iterator &, const arr::const_iterator &);
	friend bool operator>(const arr::const_iterator &, const arr::const_iterator &);
};

inline bool
operator==(const arr::const_iterator &a, const arr::const_iterator &b)
{
	return a.start == b.start;
}

inline bool
operator!=(const arr::const_iterator &a, const arr::const_iterator &b)
{
	return a.start != b.start;
}

inline bool
operator<=(const arr::const_iterator &a, const arr::const_iterator &b)
{
	return a.start <= b.start;
}

inline bool
operator>=(const arr::const_iterator &a, const arr::const_iterator &b)
{
	return a.start >= b.start;
}

inline bool
operator<(const arr::const_iterator &a, const arr::const_iterator &b)
{
	return a.start < b.start;
}

inline bool
operator>(const arr::const_iterator &a, const arr::const_iterator &b)
{
	return a.start > b.start;
}

} // namespace json
} // namespace ircd

inline bool
ircd::json::arr::contains(const string_view &s)
const
{
	return s.begin() >= this->string_view::begin() &&
	       s.end() <= this->string_view::end();
}

inline ircd::string_view
ircd::json::arr::operator[](size_t i)
const
{
	const auto it(find(i));
	return it != end()? *it : string_view{};
}

inline ircd::string_view
ircd::json::arr::at(size_t i)
const
{
	const auto it(find(i));
	return likely(it != end())? *it : throw not_found("[%zu]", i);
}

inline ircd::json::arr::const_iterator
ircd::json::arr::find(size_t i)
const
{
	auto it(begin());
	for(; it != end() && i; ++it, i--);
	return it;
}

inline size_t
ircd::json::arr::count()
const
{
	return std::distance(begin(), end());
}
