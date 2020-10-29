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

namespace ircd::json
{
	struct tuple_base;

	template<class...>
	struct tuple;

	template<class>
	struct keys;
}

/// All tuple templates inherit from this non-template type for tagging.
struct ircd::json::tuple_base
{
	// EBO
};

/// A compile-time construct to describe a JSON object's members and types.
///
/// Member access by name is O(1) because of recursive constexpr function
/// inlining when translating a name to the index number which is then used
/// as the template argument to std::get() for the value.
///
/// Here we represent a JSON object with a named tuple, allowing the programmer
/// to create a structure specifying all of the potentially valid members of the
/// object. Thus at runtime, the tuple carries around its values like a
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
struct ircd::json::tuple
:std::tuple<T...>
,tuple_base
{
	using tuple_type = std::tuple<T...>;
	using super_type = tuple<T...>;

	static constexpr size_t size() noexcept;

	operator json::value() const;
	operator crh::sha256::buf() const;

	/// For json::object constructions, the source JSON (string_view) is
	/// carried with the instance. This is important to convey additional
	/// keys not enumerated in the tuple. This will be default-initialized
	/// for other constructions when no source JSON buffer is available.
	json::object source;

	template<class name> constexpr decltype(auto) get(name&&) const noexcept;
	template<class name> constexpr decltype(auto) get(name&&) noexcept;
	template<class name> constexpr decltype(auto) at(name&&) const;
	template<class name> constexpr decltype(auto) at(name&&);

	template<class R, class name> R get(name&&, R def = {}) const noexcept;
	template<class R, class name> const R &at(name&&) const;
	template<class R, class name> R &at(name&&);

	template<class... U> explicit tuple(const tuple<U...> &);
	template<class U> explicit tuple(const json::object &, const json::keys<U> &);
	template<class U> explicit tuple(const tuple &, const json::keys<U> &);
	tuple(const json::object &);
	tuple(const json::iov &);
	tuple(const json::members &);
	tuple() = default;
};

namespace ircd {
namespace json {

template<class T>
constexpr bool
is_tuple()
noexcept
{
	return std::is_base_of<tuple_base, T>::value;
}

template<class T,
         class R>
using enable_if_tuple = typename std::enable_if<is_tuple<T>(), R>::type;

template<class T,
         class test,
         class R>
using enable_if_tuple_and = typename std::enable_if<is_tuple<T>() && test(), R>::type;

template<class T>
using tuple_type = typename T::tuple_type;

template<class T>
using tuple_size = std::tuple_size<tuple_type<T>>;

template<class T,
         size_t i>
using tuple_element = typename std::tuple_element<i, tuple_type<T>>::type;

template<class T,
         size_t i>
using tuple_value_type = typename tuple_element<T, i>::value_type;

template<class T>
inline auto &
stdcast(const T &o)
{
	return static_cast<const typename T::tuple_type &>(o);
}

template<class T>
inline auto &
stdcast(T &o)
{
	return static_cast<typename T::tuple_type &>(o);
}

template<class T>
constexpr enable_if_tuple<T, size_t>
size()
noexcept
{
	return tuple_size<T>::value;
}

} // namespace json
} // namespace ircd

#include "key.h"
#include "indexof.h"

namespace ircd {
namespace json {

template<size_t i,
         class tuple>
inline enable_if_tuple<tuple, tuple_value_type<tuple, i> &>
val(tuple &t)
noexcept
{
	return static_cast<tuple_value_type<tuple, i> &>(std::get<i>(t));
}

template<size_t i,
         class tuple>
inline enable_if_tuple<tuple, const tuple_value_type<tuple, i> &>
val(const tuple &t)
noexcept
{
	return static_cast<const tuple_value_type<tuple, i> &>(std::get<i>(t));
}

template<class tuple>
inline bool
key_exists(const string_view &key)
{
	return indexof<tuple>(key) < size<tuple>();
}

} // namespace json
} // namespace ircd

#include "for_each.h"
#include "until.h"
#include "get.h"
#include "at.h"
#include "set.h"

namespace ircd {
namespace json {

template<class... T>
template<class U>
inline
tuple<T...>::tuple(const json::object &object,
                   const json::keys<U> &keys)
:source
{
	object
}
{
	for(const auto &[key, val] : object)
		if(keys.has(key))
			set(*this, key, val);
}

template<class... T>
inline
tuple<T...>::tuple(const json::object &object)
:source
{
	object
}
{
	for(const auto &[key, val] : object)
		set(*this, key, val);
}

template<class... T>
inline
tuple<T...>::tuple(const json::iov &iov)
{
	for(const auto &[key, val] : iov)
		set(*this, key, val);
}

template<class... T>
inline
tuple<T...>::tuple(const json::members &members)
{
	for(const auto &[key, val] : members)
		set(*this, key, val);
}

template<class... T>
template<class U>
inline
tuple<T...>::tuple(const tuple &t,
                   const keys<U> &keys)
:source
{
	t.source
}
{
	for_each(t, [this, &keys]
	(const auto &key, const auto &val)
	{
		if(keys.has(key))
			set(*this, key, val);
	});
}

template<class... T>
template<class... U>
inline
tuple<T...>::tuple(const tuple<U...> &t)
:source
{
	t.source
}
{
	for_each(t, [this]
	(const auto &key, const auto &val)
	{
		set(*this, key, val);
	});
}

template<class... T>
template<class name>
constexpr decltype(auto)
tuple<T...>::at(name&& n)
{
	constexpr const size_t hash
	{
		name_hash(n)
	};

	return json::at<hash>(*this);
}

template<class... T>
template<class name>
constexpr decltype(auto)
tuple<T...>::at(name&& n)
const
{
	constexpr const size_t hash
	{
		name_hash(n)
	};

	return json::at<hash>(*this);
}

template<class... T>
template<class name>
constexpr decltype(auto)
tuple<T...>::get(name&& n)
noexcept
{
	constexpr const size_t hash
	{
		name_hash(n)
	};

	return json::get<hash>(*this);
}

template<class... T>
template<class name>
constexpr decltype(auto)
tuple<T...>::get(name&& n)
const noexcept
{
	constexpr const size_t hash
	{
		name_hash(n)
	};

	return json::get<hash>(*this);
}

template<class... T>
template<class R,
         class name>
inline R
tuple<T...>::get(name&& n,
                 R ret)
const noexcept
{
	return json::get<R>(*this, n, ret);
}

template<class... T>
template<class R,
         class name>
inline const R &
tuple<T...>::at(name&& n)
const
{
	return json::at<R>(*this, n);
}

template<class... T>
template<class R,
         class name>
inline R &
tuple<T...>::at(name&& n)
{
	return json::at<R>(*this, n);
}

template<class... T>
constexpr size_t
tuple<T...>::size()
noexcept
{
	return std::tuple_size<tuple_type>();
}

} // namespace json
} // namespace ircd

#include "_key_transform.h"
#include "keys.h"
#include "_member_transform.h"
#include "tool.h"

template<class... T>
inline ircd::json::tuple<T...>::operator
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
		[&preimage](auto&& buf)
		{
			sha256{buf, const_buffer{preimage}};
		}
	};
}

template<class... T>
inline ircd::json::tuple<T...>::operator
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
