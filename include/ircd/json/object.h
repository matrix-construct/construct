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
#define HAVE_IRCD_JSON_REFLECT_H

#define IRCD_MEMBERS(_name_, _vals_...)                   \
static constexpr const char *_member_(const size_t i)     \
{                                                         \
    constexpr const char *const val[]                     \
    {                                                     \
        _vals_                                            \
    };                                                    \
                                                          \
    return val[i];                                        \
}

namespace ircd {
namespace json {

struct basic_object
{
};

template<class... T>
struct object
:basic_object
,std::tuple<T...>
{
	using tuple_type = std::tuple<T...>;

	static constexpr size_t size()
	{
		return std::tuple_size<tuple_type>();
	}

	using std::tuple<T...>::tuple;
};

template<class object>
using tuple_type = typename object::tuple_type;

template<class object>
using tuple_size = std::tuple_size<tuple_type<object>>;

template<size_t i,
         class object>
using tuple_element = typename std::tuple_element<i, tuple_type<object>>::type;

template<class object>
constexpr auto &
tuple(const object &o)
{
	return static_cast<const typename object::tuple_type &>(o);
}

template<class object>
constexpr auto &
tuple(object &o)
{
	return static_cast<typename object::tuple_type &>(o);
}

template<class... T>
constexpr auto
size(const object<T...> &t)
{
	return t.size();
}

template<size_t i,
         class... T>
constexpr auto &
get(const object<T...> &t)
{
	return std::get<i>(t);
}

template<size_t i,
         class... T>
constexpr auto &
get(object<T...> &t)
{
	return std::get<i>(t);
}

template<size_t i,
         class object>
constexpr const char *
reflect(const object &t)
{
	return t._member_(i);
}

template<size_t i,
         class object,
         class function>
constexpr
typename std::enable_if<i == tuple_size<object>::value, void>::type
for_each(const object &t,
         function&& f)
{}

template<size_t i,
         class object,
         class function>
constexpr
typename std::enable_if<i == tuple_size<object>::value, void>::type
for_each(object &t,
         function&& f)
{}

template<size_t i = 0,
         class object,
         class function>
constexpr
typename std::enable_if<i < tuple_size<object>::value, void>::type
for_each(const object &t,
         function&& f)
{
	using type = tuple_element<i, object>;

	f(reflect<i>(t), static_cast<const type &>(get<i>(t)));
	for_each<i + 1>(t, std::forward<function>(f));
}

template<size_t i = 0,
         class object,
         class function>
constexpr
typename std::enable_if<i < tuple_size<object>::value, void>::type
for_each(object &t,
         function&& f)
{
	using type = tuple_element<i, object>;

	f(reflect<i>(t), static_cast<type &>(get<i>(t)));
	for_each<i + 1>(t, std::forward<function>(f));
}

template<class object,
         class function,
         ssize_t i>
constexpr
typename std::enable_if<(i < 0), void>::type
rfor_each(const object &t,
          function&& f)
{}

template<class object,
         class function,
         ssize_t i>
constexpr
typename std::enable_if<(i < 0), void>::type
rfor_each(object &t,
          function&& f)
{}

template<class object,
         class function,
         ssize_t i = tuple_size<object>() - 1>
constexpr
typename std::enable_if<i < tuple_size<object>(), void>::type
rfor_each(const object &t,
          function&& f)
{
	using type = tuple_element<i, object>;

	f(reflect<i>(t), static_cast<const type &>(get<i>(t)));
	rfor_each<object, function, i - 1>(t, std::forward<function>(f));
}

template<class object,
         class function,
         ssize_t i = tuple_size<object>() - 1>
constexpr
typename std::enable_if<i < tuple_size<object>(), void>::type
rfor_each(object &t,
          function&& f)
{
	using type = tuple_element<i, object>;

	f(reflect<i>(t), static_cast<type &>(get<i>(t)));
	rfor_each<object, function, i - 1>(t, std::forward<function>(f));
}

template<size_t i,
         class object,
         class function>
constexpr
typename std::enable_if<i == tuple_size<object>::value, bool>::type
until(const object &t,
      function&& f)
{
	return true;
}

template<size_t i,
         class object,
         class function>
constexpr
typename std::enable_if<i == tuple_size<object>::value, bool>::type
until(object &t,
      function&& f)
{
	return true;
}

template<size_t i = 0,
         class object,
         class function>
constexpr
typename std::enable_if<i < tuple_size<object>::value, bool>::type
until(const object &t,
      function&& f)
{
	using type = tuple_element<i, object>;

	const auto &value(static_cast<const type &>(get<i>(t)));
	return f(reflect<i>(t), value)?
	       until<i + 1>(t, std::forward<function>(f)):
	       false;
}

template<size_t i = 0,
         class object,
         class function>
constexpr
typename std::enable_if<i < tuple_size<object>::value, bool>::type
until(object &t,
      function&& f)
{
	using type = tuple_element<i, object>;

	auto &value(static_cast<type &>(get<i>(t)));
	return f(reflect<i>(t), value)?
	       until<i + 1>(t, std::forward<function>(f)):
	       false;
}

template<class object,
         class function,
         ssize_t i>
constexpr
typename std::enable_if<(i < 0), bool>::type
runtil(const object &t,
       function&& f)
{
	return true;
}

template<class object,
         class function,
         ssize_t i>
constexpr
typename std::enable_if<(i < 0), bool>::type
runtil(object &t,
       function&& f)
{
	return true;
}

template<class object,
         class function,
         ssize_t i = tuple_size<object>() - 1>
constexpr
typename std::enable_if<i < tuple_size<object>::value, bool>::type
runtil(const object &t,
       function&& f)
{
	using type = tuple_element<i, object>;

	const auto &value(static_cast<const type &>(get<i>(t)));
	return f(reflect<i>(t), value)?
	       runtil<object, function, i - 1>(t, std::forward<function>(f)):
	       false;
}

template<class object,
         class function,
         ssize_t i = tuple_size<object>() - 1>
constexpr
typename std::enable_if<i < tuple_size<object>::value, bool>::type
runtil(object &t,
       function&& f)
{
	using type = tuple_element<i, object>;

	auto &value(static_cast<type &>(get<i>(t)));
	return f(reflect<i>(t), value)?
	       runtil<object, function, i - 1>(t, std::forward<function>(f)):
	       false;
}

template<class object>
constexpr size_t
index(object&& t,
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

	return !res? ret : throw std::out_of_range("Object has no member with that name");
}

template<class object,
         class function>
constexpr bool
at(object&& t,
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

template<class object>
constexpr void
keys(object&& t,
     const std::function<void (string_view)> &f)
{
	for_each(t, [&f]
	(const char *const key, auto&& member)
	{
		f(key);
	});
}

template<class object,
         class function>
constexpr void
values(object&& t,
       function&& f)
{
	for_each(t, [&f]
	(const char *const key, auto&& member)
	{
		f(member);
	});
}

} // namespace json
} // namespace ircd
