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

// ircd::json::object is an extremely lightweight device for making
// queries into a string of JSON. This is a read-only device. It is merely
// functionality built on top of a string_view which is just a pair of
// const char* pointers to the borders of the JSON object.
//
// This class computes over strings of JSON by parsing it on-the-fly
// via forward iteration. The const_iterator is fundamental. All other member
// functions are built from this forward iteration and have worst-case linear
// complexity *every time you invoke them*. This is not necessarily a bad
// thing in the appropriate use case. Our parser is pretty efficient; this
// device conducts zero copies, zero allocations and zero indexing; instead
// the parser provides string_views to members during the iteration.
//
// The returned values are character ranges (string_view's) which themselves
// are type agnostic to their contents. The type of a value is determined at
// the user's discretion by querying the content of the string_view using a
// util function like json::type() etc. In other words, a value carries type
// data from its own original content. This means the user is responsible for
// removing prefix and suffix characters like '{' or '"' after determining the
// type if they want a truly pure value string. Our zero-copy string_view utils
// make this to a simple ballet of pointers.
//
// Other devices for dealing with strings of JSON are available: if an index
// should be populated (ircd::json::index), or if a certain set of keys
// should be found and extracted with a single pass (ircd::json::extract).
//
// Some serialization/write functions are actually provided here, these
// are to *rewrite* JSON into our desired output form.
//
// Recursive traversal cannot be achieved via a single key string value; so
// any string_view argument for a key will not be recursive. In other words,
// due to the fact that a JS identifier can have almost any character we have
// to use a different *type* like a vector of strings; in our common case we
// use an initializer_list typedef'ed as `path` and those overloads will be
// recursive.
//
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

	// fundamental
	const_iterator end() const;
	const_iterator begin() const;
	const_iterator find(const string_view &key) const;

	// util
	size_t count() const;
	bool has(const string_view &key) const;

	// returns value or default
	template<class T> T get(const string_view &key, const T &def = T{}) const;
	string_view get(const string_view &key, const string_view &def = {}) const;

	// returns value or throws not_found
	template<class T> T at(const string_view &key) const;
	string_view at(const string_view &key) const;

	// returns value or empty
	string_view operator[](const string_view &key) const;

	// rewrite-copy into string
	explicit operator std::string() const;

	// constructor
	using string_view::string_view;
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

bool has(object, const path &path);
bool has(const object &, const string_view &key);

template<class T = string_view> T get(object, const path &, const T &def = T{});
template<class T = string_view> T get(const object &, const string_view &key, const T &def = T{});

template<class T = string_view> T at(object, const path &);
template<class T = string_view> T at(const object &, const string_view &key);

object serialize(const object &, char *&buf, char *const &stop);
size_t print(char *const &buf, const size_t &max, const object &);
std::ostream &operator<<(std::ostream &, const object &);

} // namespace json
} // namespace ircd

template<class T>
T
ircd::json::at(const object &object,
               const string_view &key)
{
	return object.at<T>(key);
}

template<class T>
T
ircd::json::at(object object,
               const path &path)
{
	const auto it(std::find_if(std::begin(path), std::end(path), [&object]
	(const string_view &key)
	{
		const auto it(object.find(key));
		if(it == std::end(object))
			throw not_found("'%s'", key);

		object = it->second;
		return false;
	}));

	return lex_cast<T>(object);
}

template<class T>
T
ircd::json::get(const object &object,
                const string_view &key,
                const T &def)
{
	return object.get<T>(key, def);
}

template<class T>
T
ircd::json::get(object object,
                const path &path,
                const T &def)
{
	const auto it(std::find_if(std::begin(path), std::end(path), [&object]
	(const string_view &key)
	{
		const auto it(object.find(key));
		if(it == std::end(object))
			return true;

		object = it->second;
		return false;
	}));

	return it == std::end(path)? lex_cast<T>(object) : def;
}

inline bool
ircd::json::has(const object &object,
                const string_view &key)
{
	return object.has(key);
}

inline bool
ircd::json::has(object object,
                const path &path)
{
	const auto it(std::find_if(std::begin(path), std::end(path), [&object]
	(const string_view &key)
	{
		const auto val(object[key]);
		if(val.empty())
			return true;

		object = val;
		return false;
	}));

	// && path.size() ensures false for empty path.
	return it == std::end(path) && path.size();
}

inline ircd::string_view
ircd::json::object::operator[](const string_view &key)
const
{
	const auto it(find(key));
	return it != end()? it->second : string_view{};
}

template<class T>
T
ircd::json::object::at(const string_view &key)
const try
{
	return lex_cast<T>(at(key));
}
catch(const bad_lex_cast &e)
{
	throw type_error("'%s' must cast to type %s",
	                 key,
	                 typeid(T).name());
}

inline ircd::string_view
ircd::json::object::at(const string_view &key)
const
{
	const auto it(find(key));
	if(it == end())
		throw not_found("'%s'", key);

	return it->second;
}

template<class T>
T
ircd::json::object::get(const string_view &key,
                        const T &def)
const try
{
	const string_view sv(operator[](key));
	return !sv.empty()? lex_cast<T>(sv) : def;
}
catch(const bad_lex_cast &e)
{
	throw type_error("'%s' must cast to type %s",
	                 key,
	                 typeid(T).name());
}

inline ircd::string_view
ircd::json::object::get(const string_view &key,
                        const string_view &def)
const
{
	const string_view sv(operator[](key));
	return !sv.empty()? sv : def;
}

inline ircd::json::object::const_iterator
ircd::json::object::find(const string_view &key)
const
{
	return std::find_if(begin(), end(), [&key]
	(const auto &member)
	{
		return member.first == key;
	});
}

inline size_t
ircd::json::object::count()
const
{
	return std::distance(begin(), end());
}

inline bool
ircd::json::object::has(const string_view &key)
const
{
	return find(key) != end();
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
