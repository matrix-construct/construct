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
#define HAVE_IRCD_UTIL_TUPLE_H

//
// Utilities for std::tuple
//

namespace ircd {
inline namespace util {

template<class tuple>
constexpr bool
is_tuple()
{
	return is_specialization_of<tuple, std::tuple>::value;
}

template<class tuple>
constexpr typename std::enable_if<is_tuple<tuple>(), size_t>::type
size()
{
	return std::tuple_size<tuple>::value;
}

template<class... args>
constexpr size_t
size(const std::tuple<args...> &t)
{
	return size<std::tuple<args...>>();
}

//
// Iteration of a tuple
//
// for_each(tuple, [](auto&& elem) { ... });

template<size_t i,
         class func,
         class... args>
constexpr typename std::enable_if<i == std::tuple_size<std::tuple<args...>>::value, void>::type
for_each(std::tuple<args...> &t,
         func&& f)
{}

template<size_t i,
         class func,
         class... args>
constexpr
typename std::enable_if<i == std::tuple_size<std::tuple<args...>>::value, void>::type
for_each(const std::tuple<args...> &t,
         func&& f)
{}

template<size_t i = 0,
         class func,
         class... args>
constexpr
typename std::enable_if<i < std::tuple_size<std::tuple<args...>>::value, void>::type
for_each(const std::tuple<args...> &t,
         func&& f)
{
	f(std::get<i>(t));
	for_each<i+1>(t, std::forward<func>(f));
}

template<size_t i = 0,
         class func,
         class... args>
constexpr
typename std::enable_if<i < std::tuple_size<std::tuple<args...>>::value, void>::type
for_each(std::tuple<args...> &t,
         func&& f)
{
	f(std::get<i>(t));
	for_each<i+1>(t, std::forward<func>(f));
}

//
// Circuits for reverse iteration of a tuple
//
// rfor_each(tuple, [](auto&& elem) { ... });

template<ssize_t i,
         class func,
         class... args>
constexpr
typename std::enable_if<i == 0, void>::type
rfor_each(const std::tuple<args...> &t,
          func&& f)
{}

template<ssize_t i,
         class func,
         class... args>
constexpr
typename std::enable_if<i == 0, void>::type
rfor_each(std::tuple<args...> &t,
          func&& f)
{}

template<ssize_t i,
         class func,
         class... args>
constexpr
typename std::enable_if<(i > 0), void>::type
rfor_each(const std::tuple<args...> &t,
          func&& f)
{
	f(std::get<i - 1>(t));
	rfor_each<i - 1>(t, std::forward<func>(f));
}

template<ssize_t i,
         class func,
         class... args>
constexpr
typename std::enable_if<(i > 0), void>::type
rfor_each(std::tuple<args...> &t,
          func&& f)
{
	f(std::get<i - 1>(t));
	rfor_each<i - 1>(t, std::forward<func>(f));
}

template<ssize_t i = -1,
         class func,
         class... args>
constexpr
typename std::enable_if<(i == -1), void>::type
rfor_each(const std::tuple<args...> &t,
          func&& f)
{
	constexpr const ssize_t size
	{
		std::tuple_size<std::tuple<args...>>::value
	};

	rfor_each<size>(t, std::forward<func>(f));
}

template<ssize_t i = -1,
         class func,
         class... args>
constexpr
typename std::enable_if<(i == -1), void>::type
rfor_each(std::tuple<args...> &t,
          func&& f)
{
	constexpr const ssize_t size
	{
		std::tuple_size<std::tuple<args...>>::value
	};

	rfor_each<size>(t, std::forward<func>(f));
}

//
// Iteration of a tuple until() style: your closure returns true to continue, false
// to break. until() then remains true to the end, or returns false if not.

template<size_t i,
         class func,
         class... args>
constexpr
typename std::enable_if<i == std::tuple_size<std::tuple<args...>>::value, bool>::type
until(std::tuple<args...> &t,
      func&& f)
{
	return true;
}

template<size_t i,
         class func,
         class... args>
constexpr
typename std::enable_if<i == std::tuple_size<std::tuple<args...>>::value, bool>::type
until(const std::tuple<args...> &t,
      func&& f)
{
	return true;
}

template<size_t i = 0,
         class func,
         class... args>
constexpr
typename std::enable_if<i < std::tuple_size<std::tuple<args...>>::value, bool>::type
until(std::tuple<args...> &t,
      func&& f)
{
	using value_type = typename std::tuple_element<i, std::tuple<args...>>::type;
	return f(static_cast<value_type &>(std::get<i>(t)))? until<i+1>(t, f) : false;
}

template<size_t i = 0,
         class func,
         class... args>
constexpr
typename std::enable_if<i < std::tuple_size<std::tuple<args...>>::value, bool>::type
until(const std::tuple<args...> &t,
      func&& f)
{
	using value_type = typename std::tuple_element<i, std::tuple<args...>>::type;
	return f(static_cast<const value_type &>(std::get<i>(t)))? until<i+1>(t, f) : false;
}

//
// Circuits for reverse iteration of a tuple
//
// runtil(tuple, [](auto&& elem) -> bool { ... });

template<ssize_t i,
         class func,
         class... args>
constexpr
typename std::enable_if<i == 0, bool>::type
runtil(const std::tuple<args...> &t,
       func&& f)
{
	return true;
}

template<ssize_t i,
         class func,
         class... args>
constexpr
typename std::enable_if<i == 0, bool>::type
runtil(std::tuple<args...> &t,
       func&& f)
{
	return true;
}

template<ssize_t i,
         class func,
         class... args>
constexpr
typename std::enable_if<(i > 0), bool>::type
runtil(const std::tuple<args...> &t,
       func&& f)
{
	return f(std::get<i - 1>(t))? runtil<i - 1>(t, f) : false;
}

template<ssize_t i,
         class func,
         class... args>
constexpr typename std::enable_if<(i > 0), bool>::type
runtil(std::tuple<args...> &t,
       func&& f)
{
	return f(std::get<i - 1>(t))? runtil<i - 1>(t, f) : false;
}

template<ssize_t i = -1,
         class func,
         class... args>
constexpr typename std::enable_if<(i == -1), bool>::type
runtil(const std::tuple<args...> &t,
       func&& f)
{
	constexpr const auto size
	{
		std::tuple_size<std::tuple<args...>>::value
	};

	return runtil<size>(t, std::forward<func>(f));
}

template<ssize_t i = -1,
         class func,
         class... args>
constexpr typename std::enable_if<(i == -1), bool>::type
runtil(std::tuple<args...> &t,
       func&& f)
{
	constexpr const auto size
	{
		std::tuple_size<std::tuple<args...>>::value
	};

	return runtil<size>(t, std::forward<func>(f));
}

//
// test() is a logical inversion of until() for intuitive find()-like
// boolean semantics.
//

template<size_t i,
         class func,
         class... args>
constexpr auto
test(std::tuple<args...> &t,
     func&& f)
{
	return !until(t, [&f](auto&& arg)
	{
		return !f(arg);
	});
}

template<size_t i,
         class func,
         class... args>
constexpr auto
rtest(std::tuple<args...> &t,
      func&& f)
{
	return !runtil(t, [&f](auto&& arg)
	{
		return !f(arg);
	});
}

//
// Kronecker delta
//

template<size_t j,
         size_t i,
         class func,
         class... args>
constexpr
typename std::enable_if<i == j, void>::type
kronecker_delta(const std::tuple<args...> &t,
                func&& f)
{
	using value_type = typename std::tuple_element<i, std::tuple<args...>>::type;
	f(static_cast<const value_type &>(std::get<i>(t)));
}

template<size_t i,
         size_t j,
         class func,
         class... args>
constexpr
typename std::enable_if<i == j, void>::type
kronecker_delta(std::tuple<args...> &t,
                func&& f)
{
	using value_type = typename std::tuple_element<i, std::tuple<args...>>::type;
	f(static_cast<value_type &>(std::get<i>(t)));
}

template<size_t j,
         size_t i = 0,
         class func,
         class... args>
constexpr
typename std::enable_if<(i < j), void>::type
kronecker_delta(const std::tuple<args...> &t,
                func&& f)
{
	kronecker_delta<j, i + 1>(t, std::forward<func>(f));
}

template<size_t j,
         size_t i = 0,
         class func,
         class... args>
constexpr
typename std::enable_if<(i < j), void>::type
kronecker_delta(std::tuple<args...> &t,
                func&& f)
{
	kronecker_delta<j, i + 1>(t, std::forward<func>(f));
}

//
// Get the index of a tuple element by address at runtime
//
template<class tuple>
size_t
indexof(tuple &t, const void *const &ptr)
{
	size_t ret(0);
	const auto closure([&ret, &ptr]
	(auto &elem)
	{
		if(reinterpret_cast<const void *>(std::addressof(elem)) == ptr)
			return false;

		++ret;
		return true;
	});

	if(unlikely(until(t, closure)))
		throw std::out_of_range("no member of this tuple with that address");

	return ret;
}

//
// Tuple layouts are not standard layouts; we can only do this at runtime
//
template<size_t index,
         class tuple>
off_t
tuple_offset(const tuple &t)
{
	return
	{
	      reinterpret_cast<const uint8_t *>(std::addressof(std::get<index>(t))) -
	      reinterpret_cast<const uint8_t *>(std::addressof(t))
	};
}

} // namespace util
} // namespace ircd
