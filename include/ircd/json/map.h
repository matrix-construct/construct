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
#define HAVE_IRCD_JSON_MAP_H

namespace ircd {
namespace json {

struct map
{
	struct const_iterator;

	using key_type = string_view;
	using mapped_type = std::string;
	using value_type = std::pair<key_type, mapped_type>;
	using pointer = value_type *;
	using reference = value_type &;
	using size_type = size_t;
	using difference_type = size_t;

	std::map<std::string, std::string, std::less<>> idx;

	const_iterator begin() const;
	const_iterator end() const;
	const_iterator cbegin();
	const_iterator cend();

	bool empty() const;
	size_t size() const;

	const_iterator find(const string_view &name) const;
	bool has(const string_view &name) const;

	const val &operator[](const string_view &name) const;
	const val &at(const string_view &name) const;

	const_iterator erase(const const_iterator &s, const const_iterator &e);
	void erase(const const_iterator &s);
	bool erase(const string_view &name);

	map(const doc &d);
	map() = default;

	friend doc serialize(const map &, char *&start, char *const &stop);
	friend size_t print(char *const &buf, const size_t &max, const map &);
	friend std::ostream &operator<<(std::ostream &, const map &);
};

struct map::const_iterator
{
	using value_type = map::value_type;
	using iterator_category = std::bidirectional_iterator_tag;

  protected:
	friend class map;

	decltype(map::idx)::const_iterator it;
	mutable value_type val;

	operator const auto &() const                   { return it;                                   }

	const_iterator(const decltype(it) &it)
	:it{it}
	{}

  public:
	value_type *operator->() const                  { return &val;                                 }
	value_type &operator*() const                   { return val;                                  }

	const_iterator &operator++()                    { ++it; return *this;                          }
	const_iterator &operator--()                    { --it; return *this;                          }

	friend bool operator==(const const_iterator &a, const const_iterator &b);
	friend bool operator!=(const const_iterator &a, const const_iterator &b);
	friend bool operator<(const const_iterator &a, const const_iterator &b);
};

} // namespace json
} // namespace ircd

inline const ircd::json::val &
ircd::json::map::operator[](const string_view &name)
const
{
	return at(name);
}

inline bool
ircd::json::map::has(const string_view &name)
const
{
	return find(name) != end();
}

inline ircd::json::map::const_iterator
ircd::json::map::find(const string_view &name)
const
{
	return std::end(idx);
}

inline bool
ircd::json::map::empty()
const
{
	return idx.empty();
}

inline ircd::json::map::const_iterator
ircd::json::map::cbegin()
{
	return { std::begin(idx) };
}

inline ircd::json::map::const_iterator
ircd::json::map::cend()
{
	return { std::end(idx) };
}

inline ircd::json::map::const_iterator
ircd::json::map::begin()
const
{
	return { std::begin(idx) };
}

inline ircd::json::map::const_iterator
ircd::json::map::end()
const
{
	return { std::end(idx) };
}

inline bool
ircd::json::operator<(const map::const_iterator &a, const map::const_iterator &b)
{
	return a.it < b.it;
}

inline bool
ircd::json::operator!=(const map::const_iterator &a, const map::const_iterator &b)
{
	return a.it != b.it;
}

inline bool
ircd::json::operator==(const map::const_iterator &a, const map::const_iterator &b)
{
	return a.it == b.it;
}
