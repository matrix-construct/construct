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
#define HAVE_IRCD_JSON_INDEX_H

struct ircd::json::index
{
	struct member;
	struct const_iterator;

	using key_type = value;
	using mapped_type = value;
	using value_type = const member;
	using size_type = size_t;
	using difference_type = size_t;
	using key_compare = std::less<member>;
	using index_type = std::vector<member>;

	index_type idx;

	const_iterator begin() const;
	const_iterator end() const;
	const_iterator cbegin();
	const_iterator cend();

	bool empty() const;
	size_t count() const;
	size_t size() const;

	const_iterator find(const string_view &name) const;
	bool has(const string_view &name) const;

	const value &operator[](const string_view &name) const;
	const value &at(const string_view &name) const;

	const_iterator erase(const const_iterator &s, const const_iterator &e);
	void erase(const const_iterator &s);
	bool erase(const string_view &name);

	operator std::string() const;

	IRCD_OVERLOAD(recursive)

	index(std::initializer_list<member>);
	index(const object &d);
	index(recursive_t, const object &d);
	index() = default;
	index(index &&) = default;
	index(const index &) = delete;

	friend index &operator+=(index &, const object &);      // integration
	friend index operator+(const object &, const object &);  // integral

	friend object serialize(const index &, char *&start, char *const &stop);
	friend size_t print(char *const &buf, const size_t &max, const index &);
	friend std::ostream &operator<<(std::ostream &, const index &);
};

struct ircd::json::index::member
:std::pair<value, value>
{
	template<class K> member(const K &k, std::initializer_list<member> v);
	template<class K, class V> member(const K &k, V&& v);
	explicit member(const string_view &k);
	explicit member(const object::member &m);
	member() = default;

	friend bool operator==(const member &a, const member &b);
	friend bool operator!=(const member &a, const member &b);
	friend bool operator<(const member &a, const member &b);

	friend bool operator==(const member &a, const string_view &b);
	friend bool operator!=(const member &a, const string_view &b);
	friend bool operator<(const member &a, const string_view &b);

	friend std::ostream &operator<<(std::ostream &, const member &);
};

struct ircd::json::index::const_iterator
{
	using value_type = const member;
	using pointer = value_type *;
	using reference = value_type &;
	using difference_type = size_t;
	using iterator_category = std::bidirectional_iterator_tag;

  protected:
	friend class index;

	index::index_type::const_iterator it;

	operator const auto &() const                   { return it;                                   }

	const_iterator(const decltype(it) &it)
	:it{it}
	{}

  public:
	value_type *operator->() const                  { return it.operator->();                      }
	value_type &operator*() const                   { return it.operator*();                       }

	const_iterator &operator++()                    { ++it; return *this;                          }
	const_iterator &operator--()                    { --it; return *this;                          }

	friend bool operator==(const const_iterator &a, const const_iterator &b);
	friend bool operator!=(const const_iterator &a, const const_iterator &b);
	friend bool operator<(const const_iterator &a, const const_iterator &b);
};

inline const ircd::json::value &
ircd::json::index::operator[](const string_view &name)
const
{
	return at(name);
}

inline bool
ircd::json::index::has(const string_view &name)
const
{
	return find(name) != end();
}

inline ircd::json::index::const_iterator
ircd::json::index::find(const string_view &name)
const
{
	return std::find(std::begin(idx), std::end(idx), name);
}

inline size_t
ircd::json::index::count()
const
{
	return idx.size();
}

inline bool
ircd::json::index::empty()
const
{
	return idx.empty();
}

inline ircd::json::index::const_iterator
ircd::json::index::cbegin()
{
	return { std::begin(idx) };
}

inline ircd::json::index::const_iterator
ircd::json::index::cend()
{
	return { std::end(idx) };
}

inline ircd::json::index::const_iterator
ircd::json::index::begin()
const
{
	return { std::begin(idx) };
}

inline ircd::json::index::const_iterator
ircd::json::index::end()
const
{
	return { std::end(idx) };
}

template<class K,
         class V>
ircd::json::index::member::member(const K &k,
                                  V&& v)
:std::pair<value, value>
{
	value { k }, value { std::forward<V>(v) }
}
{}

template<class K>
ircd::json::index::member::member(const K &k,
                                  std::initializer_list<member> v)
:std::pair<value, value>
{
	value { k }, value { std::make_unique<index>(std::move(v)) }
}
{}

inline
ircd::json::index::member::member(const object::member &m)
:std::pair<value, value>
{
	m.first, value { m.second, type(m.second) }
}
{}

inline
ircd::json::index::member::member(const string_view &k)
:std::pair<value, value>
{
	k, string_view{}
}
{}

inline bool
ircd::json::operator<(const index::member &a, const index::member &b)
{
	return a.first < b.first;
}

inline bool
ircd::json::operator!=(const index::member &a, const index::member &b)
{
	return a.first != b.first;
}

inline bool
ircd::json::operator==(const index::member &a, const index::member &b)
{
	return a.first == b.first;
}

inline bool
ircd::json::operator<(const index::member &a, const string_view &b)
{
	return string_view(a.first.string, a.first.len) < b;
}

inline bool
ircd::json::operator!=(const index::member &a, const string_view &b)
{
	return string_view(a.first.string, a.first.len) != b;
}

inline bool
ircd::json::operator==(const index::member &a, const string_view &b)
{
	return string_view(a.first.string, a.first.len) == b;
}

inline bool
ircd::json::operator<(const index::const_iterator &a, const index::const_iterator &b)
{
	return a.it < b.it;
}

inline bool
ircd::json::operator!=(const index::const_iterator &a, const index::const_iterator &b)
{
	return a.it != b.it;
}

inline bool
ircd::json::operator==(const index::const_iterator &a, const index::const_iterator &b)
{
	return a.it == b.it;
}
