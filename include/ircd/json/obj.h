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
	struct delta;
	struct member;
	struct iterator;
	struct const_iterator;

	using key_type = string_view;
	using mapped_type = string_view;
	using value_type = const member;
	using size_type = size_t;
	using difference_type = std::ptrdiff_t;
	using key_compare = std::less<member>;
	using index = std::set<member>;

	doc state;
	index idx;
	bool owns_state;

	bool serialized() const;

	const_iterator begin() const;
	const_iterator end() const;
	const_iterator cbegin();
	const_iterator cend();
	iterator begin();
	iterator end();

	bool empty() const;
	size_t count() const;
	size_t size() const;

	const_iterator find(const char *const &name) const;
	iterator find(const char *const &name);

	string_view at(const char *const &name) const;
	delta at(const char *const &name);

	string_view operator[](const char *const &name) const;
	delta operator[](const char *const &name);

	obj(const doc &d);
	obj() = default;
	obj(obj &&) = default;
	explicit obj(const obj &);
	~obj() noexcept;

	friend doc serialize(const obj &, char *const &start, char *const &stop);
	friend size_t print(char *const &buf, const size_t &max, const obj &);
	friend std::ostream &operator<<(std::ostream &, const obj &);
};

struct obj::member
:doc::member
{
	bool owns_first                          { false                                          };
	bool owns_second                         { false                                          };

	using doc::member::member;
	member(const doc::member &dm)
	:doc::member{dm}
	{}
};

struct obj::delta
:string_view
{
  protected:
	struct obj *obj;
	obj::member *member;

	void commit(const string_view &);

	void set(const double &);
	void set(const uint64_t &);
	void set(const int32_t &);
	void set(const bool &);
	void set(const std::string &);
	void set(const char *const &);
	void set(const string_view &);

  public:
	delta(struct obj &, obj::member &, const string_view &);
	delta() = default;

	template<class T> delta &operator=(T&& t);
};

struct obj::iterator
{
	using proxy = std::pair<delta, delta>;
	using value_type = proxy;
	using pointer = value_type *;
	using reference = value_type &;
	using difference_type = std::ptrdiff_t;
	using iterator_category = std::output_iterator_tag;

  protected:
	friend class obj;

	struct obj *obj;
	obj::index::iterator it;
	mutable proxy state;

	iterator(struct obj &obj, obj::index::iterator it);

  public:
	auto operator==(const iterator &o) const     { return it == o.it;                              }
	auto operator!=(const iterator &o) const     { return it != o.it;                              }

	value_type *operator->();
	value_type &operator*()                      { return *operator->();                           }

	iterator &operator++();
};

struct obj::const_iterator
{
	using value_type = const member;
	using pointer = value_type *;
	using reference = value_type &;
	using difference_type = std::ptrdiff_t;
	using iterator_category = std::forward_iterator_tag;

  protected:
	friend class obj;

	obj::index::const_iterator it;

	const_iterator(const obj::index::const_iterator &it)
	:it{it}
	{}

  public:
	auto operator==(const const_iterator &o) const  { return it == o.it;                           }
	auto operator!=(const const_iterator &o) const  { return it != o.it;                           }

	value_type *operator->() const                  { return it.operator->();                      }
	value_type &operator*() const                   { return it.operator*();                       }

	const_iterator &operator++()                    { ++it; return *this;                          }
};

} // namespace json
} // namespace ircd

inline ircd::string_view
ircd::json::obj::operator[](const char *const &name)
const
{
	const auto it(find(name));
	return it != end()? it->second : string_view{};
}

inline ircd::string_view
ircd::json::obj::at(const char *const &name)
const
{
	const auto it(find(name));
	if(unlikely(it == end()))
		throw not_found("name \"%s\"", name);

	return it->second;
}

inline ircd::json::obj::iterator
ircd::json::obj::find(const char *const &name)
{
	return { *this, idx.find(string_view{name}) };
}

inline ircd::json::obj::const_iterator
ircd::json::obj::find(const char *const &name)
const
{
	return idx.find(string_view{name});
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

template<class T>
ircd::json::obj::delta &
ircd::json::obj::delta::operator=(T&& t)
{
	set(std::forward<T>(t));
	return *this;
}
