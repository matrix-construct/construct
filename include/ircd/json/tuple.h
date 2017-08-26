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
#define HAVE_IRCD_JSON_TUPLE_H

//
// Here we represent a JSON object with a named tuple, allowing the programmer
// to create a structure specifying all of the potentially valid members of the
// object. Access to a specific member is O(1) just like a native `struct`
// rather than a linear or logn lookup into a map. The size of the tuple is
// extremely minimal: only the size of the values it stores. This is because
// the member keys and type data are all static or dealt with at compile time.
//
// Create and use a tuple to efficiently extract members from a json::object.
// The tuple will populate its own members during a single-pass iteration of
// the JSON input. This is cheap, because no copies or allocations or searches
// are required to take place. If the JSON does not contain a member specified
// in the tuple, the value will be default initialized. If the JSON contains
// a member not specified in the tuple, it is ignored. If you need to know all
// of the members specified in the JSON dynamically, use a json::index or
// iterate manually.
//
namespace ircd {
namespace json {

//
// Non-template common base for all ircd::json::tuple templates.
//
struct tuple_base
{
	// class must be empty for EBO

	struct member;
};

//
// Non-template common base for all tuple members
//
struct tuple_base::member
{
};

///////////////////////////////////////////////////////////////////////////////
//
// ircd::json::tuple template. Create your own struct inheriting this
// class template with the members.
//
template<class... T>
struct tuple
:tuple_base
,std::tuple<T...>
{
	using tuple_type = std::tuple<T...>;
	using super_type = tuple<T...>;

	static constexpr size_t size();

	tuple(const json::object &);
	tuple() = default;
};

template<class... T>
constexpr size_t
tuple<T...>::size()
{
	return std::tuple_size<tuple_type>();
}

///////////////////////////////////////////////////////////////////////////////
//
// tuple member template. Specify a list of these in the tuple template to
// form the members of the object.
//
template<const char *const &name,
         class T>
struct member
:tuple_base::member
{
	using key_type = const char *;
	using value_type = T;

	static constexpr auto &key{name};
	T value;

	operator const T &() const;
	operator T &();

	member(T&& value);
	member() = default;
};

template<const char *const &name,
         class T>
member<name, T>::member(T&& value)
:value{value}
{
}

template<const char *const &name,
         class T>
member<name, T>::operator
T &()
{
	return value;
}

template<const char *const &name,
         class T>
member<name, T>::operator
const T &()
const
{
	return value;
}

///////////////////////////////////////////////////////////////////////////////
//
// Tools
//

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
constexpr size_t
size()
{
	return tuple_size<tuple>::value;
}

template<class tuple,
         size_t i>
constexpr auto &
key()
{
	return tuple_element<tuple, i>::key;
}

template<size_t i,
         class... T>
auto &
key(const tuple<T...> &t)
{
	return get<i>(t).key;
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
	const auto equal
	{
		_constexpr_equal(key<tuple, i>(), name)
	};

	return equal? i : indexof<tuple, i + 1>(name);
}

template<size_t i,
         class... T>
auto &
get(const tuple<T...> &t)
{
	return std::get<i>(t);
}

template<size_t i,
         class... T>
auto &
get(tuple<T...> &t)
{
	return std::get<i>(t);
}

template<size_t i,
         class... T>
auto &
val(const tuple<T...> &t)
{
	using value_type = tuple_value_type<tuple<T...>, i>;

	return static_cast<const value_type &>(get<i>(t));
}

template<size_t i,
         class... T>
auto &
val(tuple<T...> &t)
{
	using value_type = tuple_value_type<tuple<T...>, i>;

	return static_cast<value_type &>(get<i>(t));
}

template<const char *const &name,
         class... T>
auto &
val(const tuple<T...> &t)
{
	return val<indexof<tuple<T...>>(name)>(t);
}

template<const char *const &name,
         class... T>
auto &
val(tuple<T...> &t)
{
	return val<indexof<tuple<T...>>(name)>(t);
}

template<const char *const &name,
         class... T>
const tuple_value_type<tuple<T...>, indexof<tuple<T...>>(name)> &
at(const tuple<T...> &t)
{
	constexpr size_t idx
	{
		indexof<tuple<T...>>(name)
	};

	const auto &ret
	{
		val<idx>(t)
	};

	using value_type = tuple_value_type<tuple<T...>, idx>;

	//TODO: does tcmalloc zero this or huh?
	if(ret == value_type{})
		throw not_found("%s", name);

	return ret;
}

template<const char *const &name,
         class... T>
tuple_value_type<tuple<T...>, indexof<tuple<T...>>(name)> &
at(tuple<T...> &t)
{
	constexpr size_t idx
	{
		indexof<tuple<T...>>(name)
	};

	auto &ret
	{
		val<idx>(t)
	};

	using value_type = tuple_value_type<tuple<T...>, idx>;

	//TODO: does tcmalloc zero this or huh?
	if(ret == value_type{})
		throw not_found("%s", name);

	return ret;
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

template<class function,
         class... T>
bool
at(tuple<T...> &t,
   const string_view &name,
   function&& f)
{
	return until(t, [&name, &f]
	(const string_view &key, auto &member)
	{
		if(key != name)
			return true;

		f(member);
		return false;
	});
}

template<class function,
         class... T>
bool
at(const tuple<T...> &t,
   const string_view &name,
   function&& f)
{
	return until(t, [&name, &f]
	(const string_view &key, const auto &member)
	{
		if(key != name)
			return true;

		f(member);
		return false;
	});
}

template<class... T>
tuple<T...>::tuple(const json::object &object)
{
	//TODO: is tcmalloc zero-initializing all tuple elements, or is something else doing that?
	std::for_each(std::begin(object), std::end(object), [this]
	(const auto &member)
	{
		at(*this, member.first, [&member]
		(auto &target)
		{
			using target_type = decltype(target);
			using cast_type = typename std::remove_reference<target_type>::type; try
			{
				target = lex_cast<cast_type>(member.second);
			}
			catch(const bad_lex_cast &e)
			{
				throw parse_error("member '%s' must convert to '%s'",
				                  member.first,
				                  typeid(target_type).name());
			}
		});
	});
}

} // namespace json
} // namespace ircd
