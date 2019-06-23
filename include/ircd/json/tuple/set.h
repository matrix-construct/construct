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
#define HAVE_IRCD_JSON_TUPLE_SET_H

namespace ircd {
namespace json {

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
	d = !test(std::forward<src>(s));
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
	throw parse_error
	{
		"cannot convert '%s' to '%s'",
		demangle<src>(),
		demangle<dst>()
	};
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
	ircd::json::is_tuple<dst>() &&
	std::is_assignable<dst, src>(),
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
	ircd::json::is_tuple<dst>() &&
	!std::is_assignable<dst, src>() &&
	std::is_constructible<dst, src>(),
void>::type
_assign(dst &d,
        src&& s)
{
	d = dst{std::forward<src>(s)};
}

template<class dst,
         class src>
typename std::enable_if
<
	ircd::json::is_tuple<dst>() &&
	!std::is_assignable<dst, src>() &&
	!std::is_constructible<dst, src>(),
void>::type
#ifdef __clang__
__attribute__((unavailable("Unhandled assignment to json::tuple property")))
#else
__attribute__((error("Unhandled assignment to json::tuple property")))
#endif
_assign(dst &d,
        src&& s)
{
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
	throw parse_error
	{
		"failed to set member '%s' (from %s): %s",
		key,
		demangle<V>(),
		e.what()
	};
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
		{
			set(t, key, string_view{value});
			break;
		}

		case type::NUMBER:
		{
			if(value.floats)
				set(t, key, value.floating);
			else
				set(t, key, value.integer);
			break;
		}

		case type::OBJECT:
		case type::ARRAY:
		{
			if(unlikely(!value.serial))
				throw print_error
				{
					"Type %s must be JSON to be used by tuple member '%s'",
					reflect(type(value)),
					key
				};

			set(t, key, string_view{value});
			break;
		}
	}

	return t;
}

} // namespace json
} // namespace ircd
