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
#define HAVE_IRCD_JSON_DOC_H

namespace ircd {
namespace json {

struct doc
:string_view
{
	struct member;
	struct const_iterator;

	using key_type = string_view;
	using mapped_type = string_view;
	using value_type = const member;
	using pointer = value_type *;
	using reference = value_type &;
	using iterator = const_iterator;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using key_compare = std::less<member>;

	bool contains(const string_view &) const;

	const_iterator end() const;
	const_iterator begin() const;

	const_iterator find(const char *const &name) const;
	size_t count() const;

	string_view at(const char *const &name) const;
	string_view operator[](const char *const &name) const;

	using string_view::string_view;

	friend doc serialize(const doc &, char *&buf, char *const &stop);
	friend size_t print(char *const &buf, const size_t &max, const doc &);
	friend std::ostream &operator<<(std::ostream &, const doc &);
};

struct doc::member
:std::pair<string_view, string_view>
{
	member(const string_view &first = {}, const string_view &second = {})
	:std::pair<string_view, string_view>{first, second}
	{}

	friend bool operator==(const doc::member &, const doc::member &);
	friend bool operator<=(const doc::member &, const doc::member &);
	friend bool operator>=(const doc::member &, const doc::member &);
	friend bool operator<(const doc::member &, const doc::member &);
	friend bool operator>(const doc::member &, const doc::member &);
	friend std::ostream &operator<<(std::ostream &, const doc::member &);
};

struct doc::const_iterator
{
	using value_type = const member;
	using pointer = value_type *;
	using reference = value_type &;
	using difference_type = size_t;
	using iterator_category = std::forward_iterator_tag;

  protected:
	friend class doc;

	const char *start;
	const char *stop;
	mutable member state;

	const_iterator(const char *const &start, const char *const &stop)
	:start{start}
	,stop{stop}
	{}

  public:
	value_type *operator->() const               { return &state;                                  }
	value_type &operator*() const                { return *operator->();                           }

	const_iterator &operator++();

	friend bool operator==(const doc::const_iterator &, const doc::const_iterator &);
	friend bool operator!=(const doc::const_iterator &, const doc::const_iterator &);
	friend bool operator<=(const doc::const_iterator &, const doc::const_iterator &);
	friend bool operator>=(const doc::const_iterator &, const doc::const_iterator &);
	friend bool operator<(const doc::const_iterator &, const doc::const_iterator &);
	friend bool operator>(const doc::const_iterator &, const doc::const_iterator &);
};

inline bool
operator==(const doc::const_iterator &a, const doc::const_iterator &b)
{
	return a.start == b.start;
}

inline bool
operator!=(const doc::const_iterator &a, const doc::const_iterator &b)
{
	return a.start != b.start;
}

inline bool
operator<=(const doc::const_iterator &a, const doc::const_iterator &b)
{
	return a.start <= b.start;
}

inline bool
operator>=(const doc::const_iterator &a, const doc::const_iterator &b)
{
	return a.start >= b.start;
}

inline bool
operator<(const doc::const_iterator &a, const doc::const_iterator &b)
{
	return a.start < b.start;
}

inline bool
operator>(const doc::const_iterator &a, const doc::const_iterator &b)
{
	return a.start > b.start;
}

inline bool
operator==(const doc::member &a, const doc::member &b)
{
	return a.first == b.first;
}

inline bool
operator<=(const doc::member &a, const doc::member &b)
{
	return a.first <= b.first;
}

inline bool
operator>=(const doc::member &a, const doc::member &b)
{
	return a.first >= b.first;
}

inline bool
operator<(const doc::member &a, const doc::member &b)
{
	return a.first < b.first;
}

inline bool
operator>(const doc::member &a, const doc::member &b)
{
	return a.first > b.first;
}

} // namespace json
} // namespace ircd

inline bool
ircd::json::doc::contains(const string_view &s)
const
{
	return s.begin() >= this->string_view::begin() &&
	       s.end() <= this->string_view::end();
}

inline ircd::string_view
ircd::json::doc::operator[](const char *const &name)
const
{
	const auto it(find(name));
	return it != end()? it->second : string_view{};
}

inline ircd::string_view
ircd::json::doc::at(const char *const &name)
const
{
	const auto it(find(name));
	if(unlikely(it == end()))
		throw not_found("name \"%s\"", name);

	return it->second;
}

inline ircd::json::doc::const_iterator
ircd::json::doc::find(const char *const &name)
const
{
	return std::find_if(begin(), end(), [&name]
	(const auto &member)
	{
		return member.first == name;
	});
}

inline size_t
ircd::json::doc::count()
const
{
	return std::distance(begin(), end());
}
