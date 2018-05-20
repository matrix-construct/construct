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

//TODO: sort
template<class tuple> struct keys;

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
	using tuple_type = std::tuple<T...>;
	using super_type = tuple<T...>;
	using keys = json::keys<super_type>;

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

} // namespace json
} // namespace ircd

#include "key.h"
#include "indexof.h"

namespace ircd {
namespace json {

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

template<class tuple>
constexpr bool
key_exists(const string_view &key)
{
	return indexof<tuple>(key) < size<tuple>();
}

} // namespace json
} // namespace ircd

#include "get.h"
#include "at.h"
#include "for_each.h"
#include "until.h"
#include "set.h"

namespace ircd {
namespace json {

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
         size_t i,
         class closure>
constexpr typename std::enable_if<i == tuple::size(), it_a>::type
_key_transform(it_a it,
               const it_b &end,
               closure&& lambda)
{
	return it;
}

template<class tuple,
         class it_a,
         class it_b,
         size_t i = 0,
         class closure>
constexpr typename std::enable_if<i < tuple::size(), it_a>::type
_key_transform(it_a it,
               const it_b &end,
               closure&& lambda)
{
	if(it != end)
	{
		*it = lambda(key<tuple, i>());
		++it;
	}

	return _key_transform<tuple, it_a, it_b, i + 1>(it, end, std::move(lambda));
}

template<class tuple,
         class it_a,
         class it_b>
constexpr auto
_key_transform(it_a it,
               const it_b &end)
{
	return _key_transform<tuple>(it, end, []
	(auto&& key)
	{
		return key;
	});
}

template<class it_a,
         class it_b,
         class... T>
auto
_key_transform(const tuple<T...> &tuple,
               it_a it,
               const it_b &end)
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

} // namespace json
} // namespace ircd

#include "keys.h"

namespace ircd {
namespace json {

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
	(const auto &key, auto&& val)
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
	(auto&& ret, const auto &key, auto&& val)
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
	return _member_transform(tuple, it, end, []
	(auto&& ret, const auto &key, auto&& val) -> member
	{
		return { key, val };
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
		const json::value value(val);
		if(!defined(value))
			return false;

		ret = 1 + key.size() + 1 + 1 + serialized(value) + 1;
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
size_t
serialized(const tuple<T...> *const &b,
           const tuple<T...> *const &e)
{
	size_t ret(1 + (b == e));
	return std::accumulate(b, e, ret, []
	(size_t ret, const tuple<T...> &t)
	{
		return ret += serialized(t) + 1;
	});
}

template<class... T>
string_view
stringify(mutable_buffer &buf,
          const tuple<T...> &tuple)
{
	std::array<member, tuple.size()> members;
	const auto e{_member_transform_if(tuple, begin(members), end(members), []
	(auto &ret, const string_view &key, auto&& val)
	{
		json::value value(val);
		if(!defined(value))
			return false;

		ret = member { key, std::move(value) };
		return true;
	})};

	return stringify(buf, begin(members), e);
}

template<class... T>
string_view
stringify(mutable_buffer &buf,
          const tuple<T...> *b,
          const tuple<T...> *e)
{
	const auto start(begin(buf));
	consume(buf, copy(buf, "["_sv));
	if(b != e)
	{
		stringify(buf, *b);
		for(++b; b != e; ++b)
		{
			consume(buf, copy(buf, ","_sv));
			stringify(buf, *b);
		}
	}
	consume(buf, copy(buf, "]"_sv));
	return { start, begin(buf) };
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
