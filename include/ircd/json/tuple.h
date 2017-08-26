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

namespace ircd {
namespace json {

struct tuple_base
{
	// class must be empty for EBO
};

template<class... T>
struct tuple
:tuple_base
,std::tuple<T...>
{
	using tuple_type = std::tuple<T...>;

	static constexpr size_t size()
	{
		return std::tuple_size<tuple_type>();
	}

	using std::tuple<T...>::tuple;
};

template<class tuple>
using tuple_type = typename tuple::tuple_type;

template<class tuple>
using tuple_size = std::tuple_size<tuple_type<tuple>>;

template<size_t i,
         class tuple>
using tuple_element = typename std::tuple_element<i, tuple_type<tuple>>::type;

template<class tuple>
constexpr auto &
stdcast(const tuple &o)
{
	return static_cast<const typename tuple::tuple_type &>(o);
}

template<class tuple>
constexpr auto &
stdcast(tuple &o)
{
	return static_cast<typename tuple::tuple_type &>(o);
}

template<class... T>
constexpr auto
size(const tuple<T...> &t)
{
	return t.size();
}

template<size_t i,
         class... T>
constexpr auto &
get(const tuple<T...> &t)
{
	return std::get<i>(t);
}

template<size_t i,
         class... T>
constexpr auto &
get(tuple<T...> &t)
{
	return std::get<i>(t);
}

template<size_t i,
         class tuple>
constexpr const char *
reflect(const tuple &t)
{
	return t._member_(i);
}

#define IRCD_MEMBERS(_vals_...)                           \
static constexpr const char *_member_(const size_t i)     \
{                                                         \
    constexpr const char *const val[]                     \
    {                                                     \
        _vals_                                            \
    };                                                    \
                                                          \
    return val[i];                                        \
}

template<size_t i,
         class tuple,
         class function>
constexpr
typename std::enable_if<i == tuple_size<tuple>::value, void>::type
for_each(const tuple &t,
         function&& f)
{}

template<size_t i,
         class tuple,
         class function>
constexpr
typename std::enable_if<i == tuple_size<tuple>::value, void>::type
for_each(tuple &t,
         function&& f)
{}

template<size_t i = 0,
         class tuple,
         class function>
constexpr
typename std::enable_if<i < tuple_size<tuple>::value, void>::type
for_each(const tuple &t,
         function&& f)
{
	using type = tuple_element<i, tuple>;

	f(reflect<i>(t), static_cast<const type &>(get<i>(t)));
	for_each<i + 1>(t, std::forward<function>(f));
}

template<size_t i = 0,
         class tuple,
         class function>
constexpr
typename std::enable_if<i < tuple_size<tuple>::value, void>::type
for_each(tuple &t,
         function&& f)
{
	using type = tuple_element<i, tuple>;

	f(reflect<i>(t), static_cast<type &>(get<i>(t)));
	for_each<i + 1>(t, std::forward<function>(f));
}

template<class tuple,
         class function,
         ssize_t i>
constexpr
typename std::enable_if<(i < 0), void>::type
rfor_each(const tuple &t,
          function&& f)
{}

template<class tuple,
         class function,
         ssize_t i>
constexpr
typename std::enable_if<(i < 0), void>::type
rfor_each(tuple &t,
          function&& f)
{}

template<class tuple,
         class function,
         ssize_t i = tuple_size<tuple>() - 1>
constexpr
typename std::enable_if<i < tuple_size<tuple>(), void>::type
rfor_each(const tuple &t,
          function&& f)
{
	using type = tuple_element<i, tuple>;

	f(reflect<i>(t), static_cast<const type &>(get<i>(t)));
	rfor_each<tuple, function, i - 1>(t, std::forward<function>(f));
}

template<class tuple,
         class function,
         ssize_t i = tuple_size<tuple>() - 1>
constexpr
typename std::enable_if<i < tuple_size<tuple>(), void>::type
rfor_each(tuple &t,
          function&& f)
{
	using type = tuple_element<i, tuple>;

	f(reflect<i>(t), static_cast<type &>(get<i>(t)));
	rfor_each<tuple, function, i - 1>(t, std::forward<function>(f));
}

template<size_t i,
         class tuple,
         class function>
constexpr
typename std::enable_if<i == tuple_size<tuple>::value, bool>::type
until(const tuple &t,
      function&& f)
{
	return true;
}

template<size_t i,
         class tuple,
         class function>
constexpr
typename std::enable_if<i == tuple_size<tuple>::value, bool>::type
until(tuple &t,
      function&& f)
{
	return true;
}

template<size_t i = 0,
         class tuple,
         class function>
constexpr
typename std::enable_if<i < tuple_size<tuple>::value, bool>::type
until(const tuple &t,
      function&& f)
{
	using type = tuple_element<i, tuple>;

	const auto &value(static_cast<const type &>(get<i>(t)));
	return f(reflect<i>(t), value)?
	       until<i + 1>(t, std::forward<function>(f)):
	       false;
}

template<size_t i = 0,
         class tuple,
         class function>
constexpr
typename std::enable_if<i < tuple_size<tuple>::value, bool>::type
until(tuple &t,
      function&& f)
{
	using type = tuple_element<i, tuple>;

	auto &value(static_cast<type &>(get<i>(t)));
	return f(reflect<i>(t), value)?
	       until<i + 1>(t, std::forward<function>(f)):
	       false;
}

template<class tuple,
         class function,
         ssize_t i>
constexpr
typename std::enable_if<(i < 0), bool>::type
runtil(const tuple &t,
       function&& f)
{
	return true;
}

template<class tuple,
         class function,
         ssize_t i>
constexpr
typename std::enable_if<(i < 0), bool>::type
runtil(tuple &t,
       function&& f)
{
	return true;
}

template<class tuple,
         class function,
         ssize_t i = tuple_size<tuple>() - 1>
constexpr
typename std::enable_if<i < tuple_size<tuple>::value, bool>::type
runtil(const tuple &t,
       function&& f)
{
	using type = tuple_element<i, tuple>;

	const auto &value(static_cast<const type &>(get<i>(t)));
	return f(reflect<i>(t), value)?
	       runtil<tuple, function, i - 1>(t, std::forward<function>(f)):
	       false;
}

template<class tuple,
         class function,
         ssize_t i = tuple_size<tuple>() - 1>
constexpr
typename std::enable_if<i < tuple_size<tuple>::value, bool>::type
runtil(tuple &t,
       function&& f)
{
	using type = tuple_element<i, tuple>;

	auto &value(static_cast<type &>(get<i>(t)));
	return f(reflect<i>(t), value)?
	       runtil<tuple, function, i - 1>(t, std::forward<function>(f)):
	       false;
}

template<class tuple>
constexpr size_t
indexof(tuple&& t,
        const string_view &name)
{
	size_t ret(0);
	const auto res(until(t, [&ret, &name]
	(const string_view &key, auto&& member)
	{
		if(key == name)
			return false;

		++ret;
		return true;
	}));

	return !res? ret : throw std::out_of_range("tuple has no member with that name");
}

template<class tuple,
         class function>
constexpr bool
at(tuple&& t,
   const string_view &name,
   function&& f)
{
	return until(t, [&name, &f]
	(const string_view &key, auto&& member)
	{
		if(key != name)
			return true;

		f(member);
		return false;
	});
}

template<class tuple>
constexpr void
keys(tuple&& t,
     const std::function<void (string_view)> &f)
{
	for_each(t, [&f]
	(const char *const key, auto&& member)
	{
		f(key);
	});
}

template<class tuple,
         class function>
constexpr void
values(tuple&& t,
       function&& f)
{
	for_each(t, [&f]
	(const char *const key, auto&& member)
	{
		f(member);
	});
}

template<class tuple>
tuple &
assign_tuple(tuple &ret,
             const json::object &object)
{
	std::for_each(std::begin(object), std::end(object), [&ret](auto &&member)
	{
		at(ret, member.first, [&member](auto&& target)
		{
			using target_type = decltype(target);
			using cast_type = typename std::remove_reference<target_type>::type; try
			{
				target = lex_cast<cast_type>(member.second);
			}
			catch(const bad_lex_cast &e)
			{
				throw tuple_error("member \"%s\" must convert to '%s'",
				                  member.first,
				                  typeid(target_type).name());
			}
		});
	});

	return ret;
}

template<class tuple>
tuple
make_tuple(const json::object &object)
{
	tuple ret;
	assign_tuple(ret, object);
	return ret;
}

} // namespace json
} // namespace ircd
