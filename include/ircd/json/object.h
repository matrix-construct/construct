// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_JSON_OBJECT_H

namespace ircd::json
{
	struct object;

	bool empty(const object &);
	bool operator!(const object &);
	size_t size(const object &);
	template<name_hash_t key, class T = string_view> T at(const object &);
	template<name_hash_t key, class T = string_view> T get(const object &, const T &def = {});

	bool sorted(const object &);
	size_t serialized(const object &);
	string_view stringify(mutable_buffer &, const object &);
	std::ostream &operator<<(std::ostream &, const object &);
}

/// Lightweight interface to a JSON object string.
///
/// This makes queries into a string of JSON. This is a read-only device.
/// It is merely functionality built on top of a string_view which is just a
/// pair of `const char*` pointers to the borders of the JSON object. The first
/// character should be '{' and the last character should be '}' but this is
/// not checked on construction.
///
/// This class computes over strings of JSON by parsing it on-the-fly
/// via forward iteration. The const_iterator is fundamental. All other member
/// functions are built from this forward iteration and have worst-case linear
/// complexity *every time you invoke them*. This is not necessarily a bad
/// thing in the appropriate use case. Our parser is pretty efficient; this
/// device conducts zero copies, zero allocations and zero indexing; instead
/// the parser provides string_views to members during the iteration.
///
/// The returned values are character ranges (string_view's) which themselves
/// are type agnostic to their contents. The type of a value is determined at
/// the user's discretion by querying the content of the string_view using a
/// util function like json::type() etc. In other words, a value carries type
/// data from its own original content. This means the user is responsible for
/// removing prefix and suffix characters like '{' or '"' after determining the
/// type if they want a truly pure value string_view. Note the contrast with
/// with json::value which hides '"' around keys and string values: this object
/// preserves all characters of the value for view because it carries no other
/// type information. see: ircd::unquote().
///
/// Recursive traversal cannot be achieved via a single key string value; so
/// any string_view argument for a key will not be recursive. In other words,
/// due to the fact that a JS identifier can have almost any character we have
/// to use a different *type* like a vector of strings.
///
struct ircd::json::object
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

	static const uint max_recursion_depth;
	static const size_t max_sorted_members;

	// fundamental
	const_iterator end() const;
	const_iterator begin() const;
	const_iterator find(const string_view &key) const;
	const_iterator find(const name_hash_t &key) const;

	// util
	bool empty() const;
	size_t count() const;
	size_t size() const; // warns if used; use count()
	bool has(const string_view &key) const;

	// returns value or default
	template<class T> T get(const string_view &key, const T &def = T{}) const;
	string_view get(const string_view &key, const string_view &def = {}) const;

	// returns value or throws not_found
	template<class T = string_view> T at(const string_view &key) const;

	// returns value or empty
	string_view operator[](const string_view &key) const;

	// rewrite into allocated string copy
	explicit operator std::string() const;

	// constructor. Note that you are able to construct from invalid JSON. The
	// parser is not invoked until other operations and that's when it errors.
	using string_view::string_view;
};

namespace ircd::json
{
	bool operator==(const object::member &, const object::member &);
	bool operator!=(const object::member &, const object::member &);
	bool operator<=(const object::member &, const object::member &);
	bool operator>=(const object::member &, const object::member &);
	bool operator<(const object::member &, const object::member &);
	bool operator>(const object::member &, const object::member &);

	bool sorted(const object::member *const &, const object::member *const &);
	size_t serialized(const object::member *const &, const object::member *const &);
	size_t serialized(const object::member &);
	string_view stringify(mutable_buffer &, const object::member *const &, const object::member *const &);
	string_view stringify(mutable_buffer &, const object::member &);
	std::ostream &operator<<(std::ostream &, const object::member &);
}

struct ircd::json::object::member
:std::pair<string_view, string_view>
{
	member(const string_view &first = {}, const string_view &second = {})
	:std::pair<string_view, string_view>{first, second}
	{}
};

namespace ircd::json
{
	bool operator==(const object::const_iterator &, const object::const_iterator &);
	bool operator!=(const object::const_iterator &, const object::const_iterator &);
	bool operator<=(const object::const_iterator &, const object::const_iterator &);
	bool operator>=(const object::const_iterator &, const object::const_iterator &);
	bool operator<(const object::const_iterator &, const object::const_iterator &);
	bool operator>(const object::const_iterator &, const object::const_iterator &);
}

struct ircd::json::object::const_iterator
{
	friend class object;

	using key_type = string_view;
	using mapped_type = string_view;
	using value_type = const member;
	using pointer = value_type *;
	using reference = value_type &;
	using size_type = size_t;
	using difference_type = size_t;
	using key_compare = std::less<value_type>;
	using iterator_category = std::forward_iterator_tag;

	const char *start {nullptr};
	const char *stop {nullptr};
	member state;

	const_iterator(const char *const &start, const char *const &stop)
	:start{start}
	,stop{stop}
	{}

  public:
	value_type *operator->() const
	{
		return &state;
	}

	value_type &operator*() const
	{
		return *operator->();
	}

	const_iterator &operator++();

	const_iterator() = default;
};

template<ircd::json::name_hash_t key,
         class T>
T
ircd::json::at(const object &object)
try
{
	const auto it(object.find(key));
	if(it == end(object))
		throw not_found
		{
			"[key hash] '%zu'", key
		};

	return lex_cast<T>(it->second);
}
catch(const bad_lex_cast &e)
{
	throw type_error
	{
		"[key hash] '%zu' must cast to type %s",
		key,
		typeid(T).name()
	};
}

template<class T>
T
ircd::json::object::at(const string_view &key)
const try
{
	const auto it(find(key));
	if(it == end())
		throw not_found
		{
			"'%s'", key
		};

	return lex_cast<T>(it->second);
}
catch(const bad_lex_cast &e)
{
	throw type_error
	{
		"'%s' must cast to type %s",
		key,
		typeid(T).name()
	};
}

template<ircd::json::name_hash_t key,
         class T>
ircd::string_view
ircd::json::get(const object &object,
                const T &def)
try
{
	const auto it{object.find(key)};
	if(it == end(object))
		return def;

	const string_view sv{it->second};
	return !sv.empty()? lex_cast<T>(sv) : def;
}
catch(const bad_lex_cast &e)
{
	return def;
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
	return def;
}
