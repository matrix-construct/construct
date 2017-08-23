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
#define HAVE_IRCD_JSON_PARSE_H

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

namespace ircd {
namespace json {

struct basic_parse
{
};

template<class... T>
struct parse
:basic_parse
,std::tuple<T...>
{
	using tuple_type = std::tuple<T...>;

	static constexpr size_t size()
	{
		return std::tuple_size<tuple_type>();
	}

	parse() = default;
	template<class R> parse(R &r, const json::object &obj);
};

template<class... T>
template<class R>
parse<T...>::parse(R &r,
                   const json::object &object)
{
	std::for_each(std::begin(object), std::end(object), [this, &r]
	(auto &&member)
	{
		at(r, member.first, [&member]
		(auto&& target)
		{
			using target_type = decltype(target);
			using cast_type = typename std::remove_reference<target_type>::type; try
			{
				target = lex_cast<cast_type>(member.second);
			}
			catch(const bad_lex_cast &e)
			{
				throw parse_error("member \"%s\" must convert to '%s'",
				                  member.first,
				                  typeid(target_type).name());
			}
		});
	});
}

template<class parse>
using tuple_type = typename parse::tuple_type;

template<class parse>
using tuple_size = std::tuple_size<tuple_type<parse>>;

template<size_t i,
         class parse>
using tuple_element = typename std::tuple_element<i, tuple_type<parse>>::type;

template<class parse>
constexpr auto &
tuple(const parse &o)
{
	return static_cast<const typename parse::tuple_type &>(o);
}

template<class parse>
constexpr auto &
tuple(parse &o)
{
	return static_cast<typename parse::tuple_type &>(o);
}

template<class... T>
constexpr auto
size(const parse<T...> &t)
{
	return t.size();
}

template<size_t i,
         class... T>
constexpr auto &
get(const parse<T...> &t)
{
	return std::get<i>(t);
}

template<size_t i,
         class... T>
constexpr auto &
get(parse<T...> &t)
{
	return std::get<i>(t);
}

template<size_t i,
         class parse>
constexpr const char *
reflect(const parse &t)
{
	return t._member_(i);
}

template<size_t i,
         class parse,
         class function>
constexpr
typename std::enable_if<i == tuple_size<parse>::value, void>::type
for_each(const parse &t,
         function&& f)
{}

template<size_t i,
         class parse,
         class function>
constexpr
typename std::enable_if<i == tuple_size<parse>::value, void>::type
for_each(parse &t,
         function&& f)
{}

template<size_t i = 0,
         class parse,
         class function>
constexpr
typename std::enable_if<i < tuple_size<parse>::value, void>::type
for_each(const parse &t,
         function&& f)
{
	using type = tuple_element<i, parse>;

	f(reflect<i>(t), static_cast<const type &>(get<i>(t)));
	for_each<i + 1>(t, std::forward<function>(f));
}

template<size_t i = 0,
         class parse,
         class function>
constexpr
typename std::enable_if<i < tuple_size<parse>::value, void>::type
for_each(parse &t,
         function&& f)
{
	using type = tuple_element<i, parse>;

	f(reflect<i>(t), static_cast<type &>(get<i>(t)));
	for_each<i + 1>(t, std::forward<function>(f));
}

template<class parse,
         class function,
         ssize_t i>
constexpr
typename std::enable_if<(i < 0), void>::type
rfor_each(const parse &t,
          function&& f)
{}

template<class parse,
         class function,
         ssize_t i>
constexpr
typename std::enable_if<(i < 0), void>::type
rfor_each(parse &t,
          function&& f)
{}

template<class parse,
         class function,
         ssize_t i = tuple_size<parse>() - 1>
constexpr
typename std::enable_if<i < tuple_size<parse>(), void>::type
rfor_each(const parse &t,
          function&& f)
{
	using type = tuple_element<i, parse>;

	f(reflect<i>(t), static_cast<const type &>(get<i>(t)));
	rfor_each<parse, function, i - 1>(t, std::forward<function>(f));
}

template<class parse,
         class function,
         ssize_t i = tuple_size<parse>() - 1>
constexpr
typename std::enable_if<i < tuple_size<parse>(), void>::type
rfor_each(parse &t,
          function&& f)
{
	using type = tuple_element<i, parse>;

	f(reflect<i>(t), static_cast<type &>(get<i>(t)));
	rfor_each<parse, function, i - 1>(t, std::forward<function>(f));
}

template<size_t i,
         class parse,
         class function>
constexpr
typename std::enable_if<i == tuple_size<parse>::value, bool>::type
until(const parse &t,
      function&& f)
{
	return true;
}

template<size_t i,
         class parse,
         class function>
constexpr
typename std::enable_if<i == tuple_size<parse>::value, bool>::type
until(parse &t,
      function&& f)
{
	return true;
}

template<size_t i = 0,
         class parse,
         class function>
constexpr
typename std::enable_if<i < tuple_size<parse>::value, bool>::type
until(const parse &t,
      function&& f)
{
	using type = tuple_element<i, parse>;

	const auto &value(static_cast<const type &>(get<i>(t)));
	return f(reflect<i>(t), value)?
	       until<i + 1>(t, std::forward<function>(f)):
	       false;
}

template<size_t i = 0,
         class parse,
         class function>
constexpr
typename std::enable_if<i < tuple_size<parse>::value, bool>::type
until(parse &t,
      function&& f)
{
	using type = tuple_element<i, parse>;

	auto &value(static_cast<type &>(get<i>(t)));
	return f(reflect<i>(t), value)?
	       until<i + 1>(t, std::forward<function>(f)):
	       false;
}

template<class parse,
         class function,
         ssize_t i>
constexpr
typename std::enable_if<(i < 0), bool>::type
runtil(const parse &t,
       function&& f)
{
	return true;
}

template<class parse,
         class function,
         ssize_t i>
constexpr
typename std::enable_if<(i < 0), bool>::type
runtil(parse &t,
       function&& f)
{
	return true;
}

template<class parse,
         class function,
         ssize_t i = tuple_size<parse>() - 1>
constexpr
typename std::enable_if<i < tuple_size<parse>::value, bool>::type
runtil(const parse &t,
       function&& f)
{
	using type = tuple_element<i, parse>;

	const auto &value(static_cast<const type &>(get<i>(t)));
	return f(reflect<i>(t), value)?
	       runtil<parse, function, i - 1>(t, std::forward<function>(f)):
	       false;
}

template<class parse,
         class function,
         ssize_t i = tuple_size<parse>() - 1>
constexpr
typename std::enable_if<i < tuple_size<parse>::value, bool>::type
runtil(parse &t,
       function&& f)
{
	using type = tuple_element<i, parse>;

	auto &value(static_cast<type &>(get<i>(t)));
	return f(reflect<i>(t), value)?
	       runtil<parse, function, i - 1>(t, std::forward<function>(f)):
	       false;
}

template<class parse>
constexpr size_t
indexof(parse&& t,
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

	return !res? ret : throw std::out_of_range("parse has no member with that name");
}

template<class parse,
         class function>
constexpr bool
at(parse&& t,
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

template<class parse>
constexpr void
keys(parse&& t,
     const std::function<void (string_view)> &f)
{
	for_each(t, [&f]
	(const char *const key, auto&& member)
	{
		f(key);
	});
}

template<class parse,
         class function>
constexpr void
values(parse&& t,
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
