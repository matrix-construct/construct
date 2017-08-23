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
#define HAVE_IRCD_JSON_OBJECT_H

namespace ircd {
namespace json {

struct object
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

	const_iterator end() const;
	const_iterator begin() const;

	bool has(const string_view &name) const;
	const_iterator find(const string_view &name) const;
	size_t count() const;

	string_view at(const string_view &name) const;
	template<class T> T at(const string_view &name) const;
	string_view operator[](const string_view &name) const;
	template<class T = string_view> T get(const string_view &name, const T &def = T()) const;
	string_view get(const string_view &name, const string_view &def = {}) const;

	explicit operator std::string() const;

	using string_view::string_view;

	friend object serialize(const object &, char *&buf, char *const &stop);
	friend size_t print(char *const &buf, const size_t &max, const object &);
	friend std::ostream &operator<<(std::ostream &, const object &);
};

struct object::member
:std::pair<string_view, string_view>
{
	member(const string_view &first = {}, const string_view &second = {})
	:std::pair<string_view, string_view>{first, second}
	{}

	friend bool operator==(const member &, const member &);
	friend bool operator!=(const member &, const member &);
	friend bool operator<=(const member &, const member &);
	friend bool operator>=(const member &, const member &);
	friend bool operator<(const member &, const member &);
	friend bool operator>(const member &, const member &);

	friend std::ostream &operator<<(std::ostream &, const object::member &);
};

struct object::const_iterator
{
	using value_type = const member;
	using pointer = value_type *;
	using reference = value_type &;
	using difference_type = size_t;
	using iterator_category = std::forward_iterator_tag;

  protected:
	friend class object;

	const char *start;
	const char *stop;
	member state;

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

} // namespace json
} // namespace ircd

inline ircd::string_view
ircd::json::object::get(const string_view &name,
                     const string_view &def)
const
{
	const string_view sv(operator[](name));
	return !sv.empty()? sv : def;
}

template<class T>
T
ircd::json::object::get(const string_view &name,
                     const T &def)
const try
{
	const string_view sv(operator[](name));
	return !sv.empty()? lex_cast<T>(sv) : def;
}
catch(const bad_lex_cast &e)
{
	throw type_error("'%s' must cast to type %s", name, typeid(T).name());
}

inline ircd::string_view
ircd::json::object::operator[](const string_view &name)
const
{
	const auto p(split(name, '.'));
	const auto it(find(p.first));
	if(it == end())
		return {};

	if(!p.second.empty())
	{
		const object d(it->second);
		return d[p.second];
	}

	return it->second;
}

template<class T>
T
ircd::json::object::at(const string_view &name)
const try
{
	return lex_cast<T>(at(name));
}
catch(const bad_lex_cast &e)
{
	throw type_error("'%s' must cast to type %s", name, typeid(T).name());
}

inline ircd::string_view
ircd::json::object::at(const string_view &name)
const
{
	const auto p(split(name, '.'));
	const auto it(find(p.first));
	if(unlikely(it == end()))
		throw not_found("'%s'", p.first);

	if(!p.second.empty())
	{
		const object d(it->second);
		return d.at(p.second);
	}

	return it->second;
}

inline ircd::json::object::const_iterator
ircd::json::object::find(const string_view &name)
const
{
	return std::find_if(begin(), end(), [&name]
	(const auto &member)
	{
		return member.first == name;
	});
}

inline size_t
ircd::json::object::count()
const
{
	return std::distance(begin(), end());
}

inline bool
ircd::json::object::has(const string_view &name)
const
{
	const auto p(split(name, '.'));
	const auto it(find(p.first));
	if(it == end())
		return false;

	if(p.second.empty())
		return true;

	const object d(it->second);
	return d.has(p.second);
}

inline bool
ircd::json::operator==(const object::const_iterator &a, const object::const_iterator &b)
{
	return a.start == b.start;
}

inline bool
ircd::json::operator!=(const object::const_iterator &a, const object::const_iterator &b)
{
	return a.start != b.start;
}

inline bool
ircd::json::operator<=(const object::const_iterator &a, const object::const_iterator &b)
{
	return a.start <= b.start;
}

inline bool
ircd::json::operator>=(const object::const_iterator &a, const object::const_iterator &b)
{
	return a.start >= b.start;
}

inline bool
ircd::json::operator<(const object::const_iterator &a, const object::const_iterator &b)
{
	return a.start < b.start;
}

inline bool
ircd::json::operator>(const object::const_iterator &a, const object::const_iterator &b)
{
	return a.start > b.start;
}

inline bool
ircd::json::operator==(const object::member &a, const object::member &b)
{
	return a.first == b.first;
}

inline bool
ircd::json::operator!=(const object::member &a, const object::member &b)
{
	return a.first != b.first;
}

inline bool
ircd::json::operator<=(const object::member &a, const object::member &b)
{
	return a.first <= b.first;
}

inline bool
ircd::json::operator>=(const object::member &a, const object::member &b)
{
	return a.first >= b.first;
}

inline bool
ircd::json::operator<(const object::member &a, const object::member &b)
{
	return a.first < b.first;
}

inline bool
ircd::json::operator>(const object::member &a, const object::member &b)
{
	return a.first > b.first;
}
