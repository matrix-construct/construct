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
#define HAVE_IRCD_JSON_TUPLE_H

#include "property.h"

namespace ircd {
namespace json {

/// All tuple templates inherit from this non-template type for tagging.
struct tuple_base
{
	// EBO tag
};

/// A compile-time construct to describe a JSON object's members and types.
///
/// Member access by name is O(1) because of recursive constexpr function
/// inlining when translating a name to the index number which is then used
/// as the template argument to std::get() for the value.
///
/// Here we represent a JSON object with a named tuple, allowing the programmer
/// to create a structure specifying all of the potentially valid members of the
/// object. Thus at runtime, the tuple only carries around its values like a
/// `struct`. Unlike a `struct`, the tuple is abstractly iterable and we have
/// implemented logic operating on all JSON tuples regardless of their makeup
/// without any effort from a developer when creating a new tuple.
///
/// The member structure for the tuple is called `property` because json::member
/// is already used to pair together runtime oriented json::values.
///
/// Create and use a tuple to efficiently extract members from a json::object.
/// The tuple will populate its own members during a single-pass iteration of
/// the JSON input.
///
/// But remember, the tuple carries very little information for you at runtime
/// which may make it difficult to represent all JS phenomena like "undefined"
/// and "null".
///
template<class... T>
struct tuple
:std::tuple<T...>
,tuple_base
{
	struct keys;
	using tuple_type = std::tuple<T...>;
	using super_type = tuple<T...>;

	static constexpr size_t size();

	operator json::value() const;
	operator crh::sha256::buf() const;

	template<class... U> tuple(const tuple<U...> &);
	tuple(const json::object &);
	tuple(const json::iov &);
	tuple(const std::initializer_list<member> &);
	tuple() = default;
};

template<class tuple>
constexpr bool
is_tuple()
{
	return std::is_base_of<tuple_base, tuple>::value;
}

template<class tuple,
         class R>
using enable_if_tuple = typename std::enable_if<is_tuple<tuple>(), R>::type;

template<class tuple,
         class test,
         class R>
using enable_if_tuple_and = typename std::enable_if<is_tuple<tuple>() && test(), R>::type;

template<class tuple>
using tuple_type = typename tuple::tuple_type;

template<class tuple>
using tuple_size = std::tuple_size<tuple_type<tuple>>;

template<class tuple,
         size_t i>
using tuple_element = typename std::tuple_element<i, tuple_type<tuple>>::type;

template<class tuple,
         size_t i>
using tuple_value_type = typename tuple_element<tuple, i>::value_type;

template<class tuple>
auto &
stdcast(const tuple &o)
{
	return static_cast<const typename tuple::tuple_type &>(o);
}

template<class tuple>
auto &
stdcast(tuple &o)
{
	return static_cast<typename tuple::tuple_type &>(o);
}

template<class tuple>
constexpr enable_if_tuple<tuple, size_t>
size()
{
	return tuple_size<tuple>::value;
}

template<class tuple,
         size_t i>
constexpr enable_if_tuple<tuple, const char *const &>
key()
{
	return tuple_element<tuple, i>::key;
}

template<size_t i,
         class tuple>
enable_if_tuple<tuple, const char *const &>
key(const tuple &t)
{
	return std::get<i>(t).key;
}

template<class tuple,
         size_t hash,
         size_t i>
constexpr typename std::enable_if<i == size<tuple>(), size_t>::type
indexof()
{
	return size<tuple>();
}

template<class tuple,
         size_t hash,
         size_t i = 0>
constexpr typename std::enable_if<i < size<tuple>(), size_t>::type
indexof()
{
	constexpr auto equal
	{
		ircd::hash(key<tuple, i>()) == hash
	};

	return equal? i : indexof<tuple, hash, i + 1>();
}

template<class tuple,
         size_t i>
constexpr typename std::enable_if<i == size<tuple>(), size_t>::type
indexof(const char *const &name)
{
	return size<tuple>();
}

template<class tuple,
         size_t i = 0>
constexpr typename std::enable_if<i < size<tuple>(), size_t>::type
indexof(const char *const &name)
{
	constexpr auto equal
	{
		_constexpr_equal(key<tuple, i>(), name)
	};

	return equal? i : indexof<tuple, i + 1>(name);
}

template<class tuple,
         size_t i>
constexpr typename std::enable_if<i == size<tuple>(), size_t>::type
indexof(const string_view &name)
{
	return size<tuple>();
}

template<class tuple,
         size_t i = 0>
constexpr typename std::enable_if<i < size<tuple>(), size_t>::type
indexof(const string_view &name)
{
	const auto equal
	{
		name == key<tuple, i>()
	};

	return equal? i : indexof<tuple, i + 1>(name);
}

template<class tuple>
constexpr bool
key_exists(const string_view &key)
{
	return indexof<tuple>(key) < size<tuple>();
}

template<size_t i,
         class tuple>
enable_if_tuple<tuple, tuple_value_type<tuple, i> &>
val(tuple &t)
{
	return static_cast<tuple_value_type<tuple, i> &>(std::get<i>(t));
}

template<size_t i,
         class tuple>
enable_if_tuple<tuple, const tuple_value_type<tuple, i> &>
val(const tuple &t)
{
	return static_cast<const tuple_value_type<tuple, i> &>(std::get<i>(t));
}

template<class T>
typename std::enable_if<is_number<T>(), size_t>::type
serialized(T&& t)
{
	return lex_cast(t).size();
}

template<class T>
typename std::enable_if<is_number<T>(), bool>::type
defined(T&& t)
{
	// :-(
	using type = typename std::decay<T>::type;
	return t != std::numeric_limits<type>::max();
}

template<class dst,
         class src>
typename std::enable_if
<
	std::is_base_of<json::string, dst>() &&
	std::is_convertible<src, ircd::string_view>(),
void>::type
_assign(dst &d,
        src&& s)
{
	d = unquote(string_view{std::forward<src>(s)});
}

template<class dst,
         class src>
typename std::enable_if
<
	!std::is_base_of<json::string, dst>() &&
	std::is_convertible<src, dst>() &&
	!ircd::json::is_tuple<dst>() &&
	!std::is_same<bool, dst>(),
void>::type
_assign(dst &d,
        src&& s)
{
	d = std::forward<src>(s);
}

template<class dst,
         class src>
typename std::enable_if
<
	!std::is_base_of<json::string, dst>() &&
	std::is_convertible<src, dst>() &&
	!ircd::json::is_tuple<dst>() &&
	std::is_same<bool, dst>(),
void>::type
_assign(dst &d,
        src&& s)
{
	static const is_zero test{};
	d = test(std::forward<src>(s));
}

template<class dst,
         class src>
typename std::enable_if
<
	std::is_arithmetic<dst>() &&
	std::is_base_of<std::string_view, typename std::remove_reference<src>::type>() &&
	!std::is_base_of<ircd::byte_view<ircd::string_view>, typename std::remove_reference<src>::type>(),
void>::type
_assign(dst &d,
        src&& s)
try
{
	d = lex_cast<dst>(std::forward<src>(s));
}
catch(const bad_lex_cast &e)
{
	throw parse_error("cannot convert '%s' to '%s'",
	                  demangle<src>(),
	                  demangle<dst>());
}

template<class dst,
         class src>
typename std::enable_if
<
	std::is_arithmetic<dst>() &&
	std::is_base_of<ircd::byte_view<ircd::string_view>, typename std::remove_reference<src>::type>(),
void>::type
_assign(dst &d,
        src&& s)
{
	assert(!s.empty());
	d = byte_view<dst>(std::forward<src>(s));
}

template<class dst,
         class src>
typename std::enable_if
<
	std::is_base_of<std::string_view, dst>() &&
	std::is_pod<typename std::remove_reference<src>::type>(),
void>::type
_assign(dst &d,
        src&& s)
{
	d = byte_view<string_view>(std::forward<src>(s));
}

template<class dst,
         class src>
typename std::enable_if
<
	ircd::json::is_tuple<dst>(),
void>::type
_assign(dst &d,
        src&& s)
{
	d = dst{std::forward<src>(s)};
}

template<size_t hash,
         class tuple>
enable_if_tuple<tuple, const tuple_value_type<tuple, indexof<tuple, hash>()> &>
get(const tuple &t)
{
	constexpr size_t idx
	{
		indexof<tuple, hash>()
	};

	const auto &ret
	{
		val<idx>(t)
	};

	return ret;
}

template<size_t hash,
         class tuple>
enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, hash>()>>
get(const tuple &t,
    const tuple_value_type<tuple, indexof<tuple, hash>()> &def)
{
	constexpr size_t idx
	{
		indexof<tuple, hash>()
	};

	const auto &ret
	{
		val<idx>(t)
	};

	using value_type = tuple_value_type<tuple, idx>;

	//TODO: undefined
	return defined(ret)? ret : def;
}

template<size_t hash,
         class tuple>
enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, hash>()> &>
get(tuple &t)
{
	constexpr size_t idx
	{
		indexof<tuple, hash>()
	};

	auto &ret
	{
		val<idx>(t)
	};

	return ret;
}

template<size_t hash,
         class tuple>
enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, hash>()> &>
get(tuple &t,
    tuple_value_type<tuple, indexof<tuple, hash>()> &def)
{
	//TODO: undefined
	auto &ret{get<hash, tuple>(t)};
	using value_type = decltype(ret);
	return defined(ret)? ret : def;
}

template<size_t hash,
         class tuple>
enable_if_tuple<tuple, const tuple_value_type<tuple, indexof<tuple, hash>()> &>
at(const tuple &t)
{
	constexpr size_t idx
	{
		indexof<tuple, hash>()
	};

	const auto &ret
	{
		val<idx>(t)
	};

	using value_type = tuple_value_type<tuple, idx>;

	//TODO: XXX
	if(!defined(ret))
		throw not_found("%s", key<idx>(t));

	return ret;
}

template<size_t hash,
         class tuple>
enable_if_tuple<tuple, tuple_value_type<tuple, indexof<tuple, hash>()> &>
at(tuple &t)
{
	constexpr size_t idx
	{
		indexof<tuple, hash>()
	};

	auto &ret
	{
		val<idx>(t)
	};

	using value_type = tuple_value_type<tuple, idx>;

	//TODO: XXX
	if(!defined(ret))
		throw not_found("%s", key<idx>(t));

	return ret;
}

template<class tuple,
         class function,
         size_t i>
typename std::enable_if<i == size<tuple>(), void>::type
at(tuple &t,
   const string_view &name,
   function&& f)
{
}

template<class tuple,
         class function,
         size_t i = 0>
typename std::enable_if<i < size<tuple>(), void>::type
at(tuple &t,
   const string_view &name,
   function&& f)
{
	if(indexof<tuple>(name) == i)
		f(val<i>(t));
	else
		at<tuple, function, i + 1>(t, name, std::forward<function>(f));
}

template<class tuple,
         class function,
         size_t i>
typename std::enable_if<i == size<tuple>(), void>::type
at(const tuple &t,
   const string_view &name,
   function&& f)
{
}

template<class tuple,
         class function,
         size_t i = 0>
typename std::enable_if<i < size<tuple>(), void>::type
at(const tuple &t,
   const string_view &name,
   function&& f)
{
	if(indexof<tuple>(name) == i)
		f(val<i>(t));
	else
		at<tuple, function, i + 1>(t, name, std::forward<function>(f));
}

template<size_t i,
         class tuple,
         class function>
typename std::enable_if<i == size<tuple>(), void>::type
for_each(const tuple &t,
         function&& f)
{}

template<size_t i,
         class tuple,
         class function>
typename std::enable_if<i == size<tuple>(), void>::type
for_each(tuple &t,
         function&& f)
{}

template<size_t i = 0,
         class tuple,
         class function>
typename std::enable_if<i < size<tuple>(), void>::type
for_each(const tuple &t,
         function&& f)
{
	f(key<i>(t), val<i>(t));
	for_each<i + 1>(t, std::forward<function>(f));
}

template<size_t i = 0,
         class tuple,
         class function>
typename std::enable_if<i < size<tuple>(), void>::type
for_each(tuple &t,
         function&& f)
{
	f(key<i>(t), val<i>(t));
	for_each<i + 1>(t, std::forward<function>(f));
}

template<class tuple,
         class function>
void
for_each(const tuple &t,
         const vector_view<const string_view> &mask,
         function&& f)
{
	std::for_each(std::begin(mask), std::end(mask), [&t, &f]
	(const auto &key)
	{
		at(t, key, [&f, &key]
		(auto&& val)
		{
			f(key, val);
		});
	});
}

template<class tuple,
         class function>
void
for_each(tuple &t,
         const vector_view<const string_view> &mask,
         function&& f)
{
	std::for_each(std::begin(mask), std::end(mask), [&t, &f]
	(const auto &key)
	{
		at(t, key, [&f, &key]
		(auto&& val)
		{
			f(key, val);
		});
	});
}

template<class tuple,
         class function,
         ssize_t i>
typename std::enable_if<(i < 0), void>::type
rfor_each(const tuple &t,
          function&& f)
{}

template<class tuple,
         class function,
         ssize_t i>
typename std::enable_if<(i < 0), void>::type
rfor_each(tuple &t,
          function&& f)
{}

template<class tuple,
         class function,
         ssize_t i = size<tuple>() - 1>
typename std::enable_if<i < tuple_size<tuple>(), void>::type
rfor_each(const tuple &t,
          function&& f)
{
	f(key<i>(t), val<i>(t));
	rfor_each<tuple, function, i - 1>(t, std::forward<function>(f));
}

template<class tuple,
         class function,
         ssize_t i = size<tuple>() - 1>
typename std::enable_if<i < tuple_size<tuple>(), void>::type
rfor_each(tuple &t,
          function&& f)
{
	f(key<i>(t), val<i>(t));
	rfor_each<tuple, function, i - 1>(t, std::forward<function>(f));
}

template<size_t i,
         class tuple,
         class function>
typename std::enable_if<i == size<tuple>(), bool>::type
until(const tuple &t,
      function&& f)
{
	return true;
}

template<size_t i,
         class tuple,
         class function>
typename std::enable_if<i == size<tuple>(), bool>::type
until(tuple &t,
      function&& f)
{
	return true;
}

template<size_t i = 0,
         class tuple,
         class function>
typename std::enable_if<i < size<tuple>(), bool>::type
until(const tuple &t,
      function&& f)
{
	return f(key<i>(t), val<i>(t))?
	       until<i + 1>(t, std::forward<function>(f)):
	       false;
}

template<size_t i = 0,
         class tuple,
         class function>
typename std::enable_if<i < size<tuple>(), bool>::type
until(tuple &t,
      function&& f)
{
	return f(key<i>(t), val<i>(t))?
	       until<i + 1>(t, std::forward<function>(f)):
	       false;
}

template<size_t i,
         class tuple,
         class function>
typename std::enable_if<i == size<tuple>(), bool>::type
until(const tuple &a,
      const tuple &b,
      function&& f)
{
	return true;
}

template<size_t i = 0,
         class tuple,
         class function>
typename std::enable_if<i < size<tuple>(), bool>::type
until(const tuple &a,
      const tuple &b,
      function&& f)
{
	return f(key<i>(a), val<i>(a), val<i>(b))?
	       until<i + 1>(a, b, std::forward<function>(f)):
	       false;
}

template<class tuple,
         class function,
         ssize_t i>
typename std::enable_if<(i < 0), bool>::type
runtil(const tuple &t,
       function&& f)
{
	return true;
}

template<class tuple,
         class function,
         ssize_t i>
typename std::enable_if<(i < 0), bool>::type
runtil(tuple &t,
       function&& f)
{
	return true;
}

template<class tuple,
         class function,
         ssize_t i = size<tuple>() - 1>
typename std::enable_if<i < size<tuple>(), bool>::type
runtil(const tuple &t,
       function&& f)
{
	return f(key<i>(t), val<i>(t))?
	       runtil<tuple, function, i - 1>(t, std::forward<function>(f)):
	       false;
}

template<class tuple,
         class function,
         ssize_t i = size<tuple>() - 1>
typename std::enable_if<i < size<tuple>(), bool>::type
runtil(tuple &t,
       function&& f)
{
	return f(key<i>(t), val<i>(t))?
	       runtil<tuple, function, i - 1>(t, std::forward<function>(f)):
	       false;
}

template<class V,
         class... T>
tuple<T...> &
set(tuple<T...> &t,
    const string_view &key,
    V&& val)
try
{
	at(t, key, [&key, &val]
	(auto &target)
	{
		_assign(target, std::forward<V>(val));
	});

	return t;
}
catch(const std::exception &e)
{
	throw parse_error("failed to set member '%s' (from %s): %s",
	                  key,
	                  demangle<V>(),
	                  e.what());
}

template<class... T>
tuple<T...> &
set(tuple<T...> &t,
    const string_view &key,
    const json::value &value)
{
	switch(type(value))
	{
		case type::STRING:
		case type::LITERAL:
			set(t, key, string_view{value});
			break;

		case type::NUMBER:
			if(value.floats)
				set(t, key, value.floating);
			else
				set(t, key, value.integer);
			break;

		case type::OBJECT:
		case type::ARRAY:
			if(unlikely(!value.serial))
				throw print_error("Type %s must be JSON to be used by tuple member '%s'",
				                  reflect(type(value)),
				                  key);

			set(t, key, string_view{value});
			break;
	}

	return t;
}

template<class... T>
tuple<T...>::tuple(const json::object &object)
{
	std::for_each(std::begin(object), std::end(object), [this]
	(const auto &member)
	{
		set(*this, member.first, member.second);
	});
}

template<class... T>
tuple<T...>::tuple(const json::iov &iov)
{
	std::for_each(std::begin(iov), std::end(iov), [this]
	(const auto &member)
	{
		set(*this, member.first, member.second);
	});
}

template<class... T>
tuple<T...>::tuple(const std::initializer_list<member> &members)
{
	std::for_each(std::begin(members), std::end(members), [this]
	(const auto &member)
	{
		set(*this, member.first, member.second);
	});
}

template<class... T>
template<class... U>
tuple<T...>::tuple(const tuple<U...> &t)
{
	for_each(t, [this]
	(const auto &key, const auto &val)
	{
		set(*this, key, val);
	});
}

template<class... T>
constexpr size_t
tuple<T...>::size()
{
	return std::tuple_size<tuple_type>();
}

template<class tuple,
         class it_a,
         class it_b,
         class closure>
constexpr auto
_key_transform(it_a it,
               const it_b end,
               closure&& lambda)
{
	for(size_t i(0); i < tuple::size() && it != end; ++i)
	{
		*it = lambda(key<tuple, i>());
		++it;
	}

	return it;
}

template<class tuple,
         class it_a,
         class it_b>
constexpr auto
_key_transform(it_a it,
               const it_b end)
{
	for(size_t i(0); i < tuple::size() && it != end; ++i)
	{
		*it = key<tuple, i>();
		++it;
	}

	return it;
}

template<class it_a,
         class it_b,
         class... T>
auto
_key_transform(const tuple<T...> &tuple,
               it_a it,
               const it_b end)
{
	for_each(tuple, [&it, &end]
	(const auto &key, const auto &val)
	{
		if(it != end)
		{
			*it = key;
			++it;
		}
	});

	return it;
}

template<class... T>
struct tuple<T...>::keys
:std::array<string_view, tuple<T...>::size()>
{
	struct selection;
	struct include;
	struct exclude;

	constexpr keys()
	{
		_key_transform<tuple<T...>>(this->begin(), this->end());
	}
};

template<class... T>
struct tuple<T...>::keys::selection
:std::bitset<tuple<T...>::size()>
{
	template<class closure>
	constexpr bool until(closure &&function) const
	{
		for(size_t i(0); i < this->size(); ++i)
			if(this->test(i))
				if(!function(key<tuple<T...>, i>()))
					return false;

		return true;
	}

	template<class closure>
	constexpr void for_each(closure &&function) const
	{
		this->until([&function](auto&& key)
		{
			function(key);
			return true;
		});
	}

	template<class it_a,
	         class it_b>
	constexpr auto transform(it_a it, const it_b end) const
	{
		this->until([&it, &end](auto&& key)
		{
			if(it == end)
				return false;

			*it = key;
			++it;
			return true;
		});
	}
};

template<class... T>
struct tuple<T...>::keys::include
:selection
{
	constexpr include(const std::initializer_list<string_view> &list)
	{
		for(const auto &key : list)
			this->set(indexof<tuple<T...>>(key), true);
	}
};

template<class... T>
struct tuple<T...>::keys::exclude
:selection
{
	constexpr exclude(const std::initializer_list<string_view> &list)
	{
		this->set();
		for(const auto &key : list)
			this->set(indexof<tuple<T...>>(key), false);
	}
};

template<class it_a,
         class it_b,
         class closure,
         class... T>
auto
_member_transform_if(const tuple<T...> &tuple,
                     it_a it,
                     const it_b end,
                     closure&& lambda)
{
	until(tuple, [&it, &end, &lambda]
	(const auto &key, const auto &val)
	{
		if(it == end)
			return false;

		if(lambda(*it, key, val))
			++it;

		return true;
	});

	return it;
}

template<class it_a,
         class it_b,
         class closure,
         class... T>
auto
_member_transform(const tuple<T...> &tuple,
                  it_a it,
                  const it_b end,
                  closure&& lambda)
{
	return _member_transform_if(tuple, it, end, [&lambda]
	(auto&& ret, const auto &key, const auto &val)
	{
		ret = lambda(key, val);
		return true;
	});
}

template<class it_a,
         class it_b,
         class... T>
auto
_member_transform(const tuple<T...> &tuple,
                  it_a it,
                  const it_b end)
{
	return _member_transform_if(tuple, it, end, []
	(auto&& ret, const auto &key, const auto &val)
	{
		ret = member { key, val };
		return true;
	});
}

template<class... T>
size_t
serialized(const tuple<T...> &t)
{
	constexpr const size_t member_count
	{
		tuple<T...>::size()
	};

	std::array<size_t, member_count> sizes {0};
	const auto e{_member_transform_if(t, begin(sizes), end(sizes), []
	(auto &ret, const string_view &key, auto&& val)
	{
		if(!defined(val))
			return false;

		//    "                "   :                     ,
		ret = 1 + key.size() + 1 + 1 + serialized(val) + 1;
		return true;
	})};

	// Subtract one to get the final size when an extra comma is
	// accumulated on non-empty objects.
	const auto overhead
	{
		1 + std::all_of(begin(sizes), e, is_zero{})
	};

	return std::accumulate(begin(sizes), e, size_t(overhead));
}

template<class... T>
string_view
stringify(mutable_buffer &buf,
          const tuple<T...> &tuple)
{
	std::array<member, tuple.size()> members;
	std::sort(begin(members), end(members), []
	(const auto &a, const auto &b)
	{
		return a.first < b.first;
	});

	const auto e{_member_transform_if(tuple, begin(members), end(members), []
	(auto &ret, const string_view &key, auto&& val)
	{
		if(!defined(val))
			return false;

		ret = member { key, val };
		return true;
	})};

	return stringify(buf, begin(members), e);
}

template<class... T>
std::ostream &
operator<<(std::ostream &s, const tuple<T...> &t)
{
    s << json::strung(t);
    return s;
}

template<class... T>
tuple<T...>::operator
crh::sha256::buf()
const
{
	//TODO: XXX
	const auto preimage
	{
		json::strung(*this)
	};

	return crh::sha256::buf
	{
		[&preimage](auto &buf)
		{
			sha256{buf, const_buffer{preimage}};
		}
	};
}

template<class tuple>
enable_if_tuple<tuple, ed25519::sig>
sign(const tuple &t,
     const ed25519::sk &sk)
{
	//TODO: XXX
	const auto preimage
	{
		json::strung(t)
	};

	return ed25519::sig
	{
		[&sk, &preimage](auto &buf)
		{
			sk.sign(buf, const_buffer{preimage});
		}
	};
}

template<class tuple>
enable_if_tuple<tuple, bool>
verify(const tuple &t,
       const ed25519::pk &pk,
       const ed25519::sig &sig,
       std::nothrow_t)
noexcept try
{
	//TODO: XXX
	const auto preimage
	{
		json::strung(t)
	};

	return pk.verify(const_buffer{preimage}, sig);
}
catch(const std::exception &e)
{
	log::error("Verification of json::tuple unexpected failure: %s", e.what());
	return false;
}

template<class tuple>
enable_if_tuple<tuple, void>
verify(const tuple &t,
       const ed25519::pk &pk,
       const ed25519::sig &sig)
{
	if(!verify(t, pk, sig, std::nothrow))
		throw ed25519::bad_sig{"Verification failed"};
}

template<class tuple>
enable_if_tuple<tuple, void>
verify(const tuple &t,
       const ed25519::pk &pk)
{
	const ed25519::sig sig
	{
		[&t](auto &buf)
		{
			b64decode(buf, at<"signatures"_>(t));
		}
	};

	auto copy(t);
	at<"signatures"_>(copy) = string_view{};
	if(!verify(copy, pk, sig, std::nothrow))
		throw ed25519::bad_sig{"Verification failed"};
}

template<class... T>
tuple<T...>::operator
json::value()
const
{
	json::value ret;
	ret.type = OBJECT;
	ret.create_string(serialized(*this), [this]
	(mutable_buffer buffer)
	{
		stringify(buffer, *this);
	});
	return ret;

}

} // namespace json
} // namespace ircd
