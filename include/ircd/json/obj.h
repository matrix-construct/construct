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
#define HAVE_IRCD_JSON_OBJ_H

namespace ircd {
namespace json {

struct obj
{
	struct member;
	struct const_iterator;

	using key_type = val;
	using mapped_type = val;
	using value_type = const member;
	using size_type = size_t;
	using difference_type = size_t;
	using key_compare = std::less<member>;
	using index = std::vector<member>;

	index idx;

	const_iterator begin() const;
	const_iterator end() const;
	const_iterator cbegin();
	const_iterator cend();

	bool empty() const;
	size_t count() const;
	size_t size() const;

	const_iterator find(const string_view &name) const;
	bool has(const string_view &name) const;

	const val &operator[](const string_view &name) const;
	const val &at(const string_view &name) const;

	const_iterator erase(const const_iterator &s, const const_iterator &e);
	void erase(const const_iterator &s);
	bool erase(const string_view &name);

	explicit operator std::string() const;

	IRCD_OVERLOAD(merge)

	obj(std::initializer_list<member>);
	obj(const doc &d, const bool &recurse = false);
	obj(merge_t, const std::initializer_list<doc> &);
	obj() = default;
	obj(obj &&) = default;
	obj(const obj &) = delete;

	friend doc serialize(const obj &, char *&start, char *const &stop);
	friend size_t print(char *const &buf, const size_t &max, const obj &);
	friend std::ostream &operator<<(std::ostream &, const obj &);
};

struct obj::member
:std::pair<val, val>
{
	template<class K> member(const K &k, std::initializer_list<member> v);
	template<class K, class V> member(const K &k, V&& v);
	explicit member(const string_view &k);
	explicit member(const doc::member &m);
	member() = default;

	friend bool operator==(const member &a, const member &b);
	friend bool operator!=(const member &a, const member &b);
	friend bool operator<(const member &a, const member &b);

	friend bool operator==(const member &a, const string_view &b);
	friend bool operator!=(const member &a, const string_view &b);
	friend bool operator<(const member &a, const string_view &b);

	friend std::ostream &operator<<(std::ostream &, const member &);
};

struct obj::const_iterator
{
	using value_type = const member;
	using pointer = value_type *;
	using reference = value_type &;
	using difference_type = size_t;
	using iterator_category = std::forward_iterator_tag;

  protected:
	friend class obj;

	obj::index::const_iterator it;

	operator const auto &() const                   { return it;                                   }

	const_iterator(const decltype(it) &it)
	:it{it}
	{}

  public:
	value_type *operator->() const                  { return it.operator->();                      }
	value_type &operator*() const                   { return it.operator*();                       }

	const_iterator &operator++()                    { ++it; return *this;                          }

	friend bool operator==(const const_iterator &a, const const_iterator &b);
	friend bool operator!=(const const_iterator &a, const const_iterator &b);
	friend bool operator<(const const_iterator &a, const const_iterator &b);
};

} // namespace json
} // namespace ircd

inline const ircd::json::val &
ircd::json::obj::operator[](const string_view &name)
const
{
	return at(name);
}

inline bool
ircd::json::obj::has(const string_view &name)
const
{
	return find(name) != end();
}

inline ircd::json::obj::const_iterator
ircd::json::obj::find(const string_view &name)
const
{
	return std::find(std::begin(idx), std::end(idx), name);
}

inline size_t
ircd::json::obj::count()
const
{
	return idx.size();
}

inline bool
ircd::json::obj::empty()
const
{
	return idx.empty();
}

inline ircd::json::obj::const_iterator
ircd::json::obj::cbegin()
{
	return { std::begin(idx) };
}

inline ircd::json::obj::const_iterator
ircd::json::obj::cend()
{
	return { std::end(idx) };
}

inline ircd::json::obj::const_iterator
ircd::json::obj::begin()
const
{
	return { std::begin(idx) };
}

inline ircd::json::obj::const_iterator
ircd::json::obj::end()
const
{
	return { std::end(idx) };
}

template<class K,
         class V>
ircd::json::obj::member::member(const K &k,
                                V&& v)
:std::pair<val, val>
{
	val { k }, val { std::forward<V>(v) }
}
{}

template<class K>
ircd::json::obj::member::member(const K &k,
                                std::initializer_list<member> v)
:std::pair<val, val>
{
	val { k }, val { std::make_unique<obj>(std::move(v)) }
}
{}

inline
ircd::json::obj::member::member(const doc::member &m)
:std::pair<val, val>
{
	m.first, val { m.second, type(m.second) }
}
{}

inline
ircd::json::obj::member::member(const string_view &k)
:std::pair<val, val>
{
	k, string_view{}
}
{}

inline bool
ircd::json::operator<(const obj::member &a, const obj::member &b)
{
	return a.first < b.first;
}

inline bool
ircd::json::operator!=(const obj::member &a, const obj::member &b)
{
	return a.first != b.first;
}

inline bool
ircd::json::operator==(const obj::member &a, const obj::member &b)
{
	return a.first == b.first;
}

inline bool
ircd::json::operator<(const obj::member &a, const string_view &b)
{
	return string_view(a.first.string, a.first.len) < b;
}

inline bool
ircd::json::operator!=(const obj::member &a, const string_view &b)
{
	return string_view(a.first.string, a.first.len) != b;
}

inline bool
ircd::json::operator==(const obj::member &a, const string_view &b)
{
	return string_view(a.first.string, a.first.len) == b;
}

inline bool
ircd::json::operator<(const obj::const_iterator &a, const obj::const_iterator &b)
{
	return a.it < b.it;
}

inline bool
ircd::json::operator!=(const obj::const_iterator &a, const obj::const_iterator &b)
{
	return a.it != b.it;
}

inline bool
ircd::json::operator==(const obj::const_iterator &a, const obj::const_iterator &b)
{
	return a.it == b.it;
}
