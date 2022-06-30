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

template<size_t i = 0,
         class func,
         class... args>
constexpr inline bool
for_each(const std::tuple<args...> &t,
         func&& f)
{
	if constexpr(i < size<std::tuple<args...>>())
	{
		using closure_result = std::invoke_result_t
		<
			decltype(f), decltype(std::get<i>(t))
		>;

		constexpr bool terminable
		{
			std::is_same<closure_result, bool>()
		};

		if constexpr(terminable)
		{
			if(!f(std::get<i>(t)))
				return false;
		}
		else f(std::get<i>(t));

		return for_each<i + 1, func, args...>(t, std::forward<func>(f));
	}
	else return true;
}

template<size_t i = 0,
         class func,
         class... args>
constexpr inline bool
for_each(std::tuple<args...> &t,
         func&& f)
{
	if constexpr(i < size<std::tuple<args...>>())
	{
		using closure_result = std::invoke_result_t
		<
			decltype(f), decltype(std::get<i>(t))
		>;

		constexpr bool terminable
		{
			std::is_same<closure_result, bool>()
		};

		if constexpr(terminable)
		{
			if(!f(std::get<i>(t)))
				return false;
		}
		else f(std::get<i>(t));

		return for_each<i + 1, func, args...>(t, std::forward<func>(f));
	}
	else return true;
}

//
// Circuits for reverse iteration of a tuple
//
// rfor_each(tuple, [](auto&& elem) { ... });

template<class func,
         class... args,
         ssize_t i = size<std::tuple<args...>>() - 1>
constexpr inline bool
rfor_each(const std::tuple<args...> &t,
          func&& f)
{
	if constexpr(i >= 0)
	{
		using closure_result = std::invoke_result_t
		<
			decltype(f), decltype(std::get<i>(t))
		>;

		constexpr bool terminable
		{
			std::is_same<closure_result, bool>()
		};

		if constexpr(terminable)
		{
			if(!f(std::get<i>(t)))
				return false;
		}
		else f(std::get<i>(t));

		return rfor_each<func, args..., i - 1>(t, std::forward<func>(f));
	}
	else return true;
}

template<class func,
         class... args,
         ssize_t i = size<std::tuple<args...>>() - 1>
constexpr inline bool
rfor_each(std::tuple<args...> &t,
          func&& f)
{
	if constexpr(i >= 0)
	{
		using closure_result = std::invoke_result_t
		<
			decltype(f), decltype(std::get<i>(t))
		>;

		constexpr bool terminable
		{
			std::is_same<closure_result, bool>()
		};

		if constexpr(terminable)
		{
			if(!f(std::get<i>(t)))
				return false;
		}
		else f(std::get<i>(t));

		return rfor_each<func, args..., i - 1>(t, std::forward<func>(f));
	}
	else return true;
}

//
// test() is a logical inversion of for_each() for intuitive find()-like
// boolean semantics.
//

template<class func,
         class... args>
constexpr inline auto
test(const std::tuple<args...> &t,
     func&& f)
{
	return !for_each(t, [&f](const auto &arg)
	{
		return !f(arg);
	});
}

template<class func,
         class... args>
constexpr inline auto
rtest(const std::tuple<args...> &t,
      func&& f)
{
	return !rfor_each(t, [&f](const auto &arg)
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
constexpr typename std::enable_if<i == j, void>::type
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
constexpr typename std::enable_if<i == j, void>::type
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
constexpr typename std::enable_if<(i < j), void>::type
kronecker_delta(const std::tuple<args...> &t,
                func&& f)
{
	kronecker_delta<j, i + 1>(t, std::forward<func>(f));
}

template<size_t j,
         size_t i = 0,
         class func,
         class... args>
constexpr typename std::enable_if<(i < j), void>::type
kronecker_delta(std::tuple<args...> &t,
                func&& f)
{
	kronecker_delta<j, i + 1>(t, std::forward<func>(f));
}

/// Get the index of a tuple element by address at runtime
template<class tuple>
inline size_t
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

//// Tuple layouts are not standard layouts; we can only do this at runtime
template<size_t index,
         class tuple>
inline off_t
tuple_offset(const tuple &t)
{
	return off_t
	{
	      reinterpret_cast<const uint8_t *>(std::addressof(std::get<index>(t))) -
	      reinterpret_cast<const uint8_t *>(std::addressof(t))
	};
}

} // namespace util
} // namespace ircd
