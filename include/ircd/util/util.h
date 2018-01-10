// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#pragma once
#define HAVE_IRCD_UTIL_H

namespace ircd
{
	/// Utilities for IRCd.
	///
	/// This is an inline namespace: everything declared in it will be
	/// accessible in ircd::. By first opening it here as inline all
	/// subsequent openings of this namespace do not have to use the inline
	/// keyword but will still be inlined to ircd::.
	inline namespace util {}
}

//
// Fundamental macros
//

#define IRCD_EXPCAT(a, b)   a ## b
#define IRCD_CONCAT(a, b)   IRCD_EXPCAT(a, b)
#define IRCD_UNIQUE(a)      IRCD_CONCAT(a, __COUNTER__)

/// Macro to arrange a function overload scheme based on the following
/// convention: An available `name` is chosen, from this name a strong type
/// is created by appending `_t`. The name itself becomes a static constexpr
/// instance of this `name_t`. Functions can be declared with an argument
/// accepting `name_t`, and called by passing `name`
///
/// IRCD_OVERLOAD(foo)                             // declare overload
/// void function(int, foo_t) {}                   // overloaded version
/// void function(int) { function(0, foo); }       // calls overloaded version
/// function(0);                                   // calls regular version
///
#define IRCD_OVERLOAD(NAME) \
    static constexpr struct NAME##_t {} NAME {};

/// Imports an overload scheme from elsewhere without redeclaring the type_t.
#define IRCD_USING_OVERLOAD(ALIAS, ORIGIN) \
    static constexpr const auto &ALIAS{ORIGIN}

//
// Typedef macros
//

/// Creates a type `NAME` from original type `TYPE` by inheriting from `TYPE`
/// and passing through construction to `TYPE`. These implicit conversions
/// we consider to be a "weak" typedef
#define IRCD_WEAK_TYPEDEF(TYPE, NAME)                       \
struct NAME                                                 \
:TYPE                                                       \
{                                                           \
    using TYPE::TYPE;                                       \
};

/// Creates a type `NAME` by wrapping instance of `TYPE` as a member and
/// providing explicit conversions to `TYPE` and aggregate construction only. We
/// consider this a "strong" typedef which is useful for wrapping POD types
/// for overloaded functions, etc.
#define IRCD_STRONG_TYPEDEF(TYPE, NAME)                       \
struct NAME                                                   \
{                                                             \
    TYPE val;                                                 \
                                                              \
    explicit operator const TYPE &() const  { return val;  }  \
    explicit operator TYPE &()              { return val;  }  \
};

/// Convenience for weak typedef statements
#define IRCD_WEAK_T(TYPE) \
    IRCD_WEAK_TYPEDEF(TYPE, IRCD_UNIQUE(weak_t))

/// Convenience for strong typedef statements
/// ex: using foo_t = IRCD_STRONG_T(int)
#define IRCD_STRONG_T(TYPE) \
    IRCD_STRONG_TYPEDEF(TYPE, IRCD_UNIQUE(strong_t))

//
// Debug size of structure at compile time.
//

/// Internal use only
template<size_t SIZE>
struct _TEST_SIZEOF_;

/// Output the sizeof a structure at compile time.
/// This stops the compiler with an error (good) containing the size of the target
/// in the message.
///
/// example: struct foo {}; IRCD_TEST_SIZEOF(foo)
///
#define IRCD_TEST_SIZEOF(name) \
	ircd::util::_TEST_SIZEOF_<sizeof(name)> _test_;


/// A standard unique_ptr but accepting an std::function for T as its custom
/// deleter. This reduces the boilerplate burden on declaring the right
/// unique_ptr template for custom deleters every single time.
///
template<class T>
using custom_ptr = std::unique_ptr<T, std::function<void (T *) noexcept>>;


#include "unit_literal.h"
#include "unwind.h"
#include "reentrance.h"
#include "enum.h"
#include "syscall.h"
#include "va_rtti.h"
#include "unique_iterator.h"
#include "instance_list.h"
#include "bswap.h"

// Unsorted section
namespace ircd {
namespace util {

inline size_t
size(std::ostream &s)
{
	const auto cur(s.tellp());
	s.seekp(0, std::ios::end);
	const auto ret(s.tellp());
	s.seekp(cur, std::ios::beg);
	return ret;
}

template<size_t SIZE>
constexpr size_t
size(const char (&buf)[SIZE])
{
	return SIZE;
}

template<size_t SIZE>
constexpr size_t
size(const std::array<const char, SIZE> &buf)
{
	return SIZE;
}

template<size_t SIZE>
constexpr size_t
size(const std::array<char, SIZE> &buf)
{
	return SIZE;
}

template<class T>
constexpr typename std::enable_if<std::is_integral<T>::value, size_t>::type
size(const T &val)
{
	return sizeof(T);
}


template<size_t SIZE>
constexpr const char *
data(const char (&buf)[SIZE])
{
	return buf;
}

template<size_t SIZE>
constexpr char *
data(char (&buf)[SIZE])
{
	return buf;
}

template<class T>
constexpr typename std::enable_if<std::is_pod<T>::value, const uint8_t *>::type
data(const T &val)
{
	return reinterpret_cast<const uint8_t *>(&val);
}

template<class T>
constexpr typename std::enable_if<std::is_pod<T>::value, uint8_t *>::type
data(T &val)
{
	return reinterpret_cast<uint8_t *>(&val);
}


template<class T>
auto
string(const T &s)
{
	std::stringstream ss;
	return static_cast<std::stringstream &>(ss << s).str();
}

inline auto
string(const char *const &buf, const size_t &size)
{
	return std::string{buf, size};
}

inline auto
string(const uint8_t *const &buf, const size_t &size)
{
	return string(reinterpret_cast<const char *>(buf), size);
}


//
// stringstream buffer set macros
//

inline std::string &
pubsetbuf(std::stringstream &ss,
          std::string &s)
{
	auto *const &data
	{
		const_cast<char *>(s.data())
	};

	ss.rdbuf()->pubsetbuf(data, s.size());
	return s;
}

inline std::string &
pubsetbuf(std::stringstream &ss,
          std::string &s,
          const size_t &size)
{
	s.resize(size, char{});
	return pubsetbuf(ss, s);
}

inline std::string &
resizebuf(std::stringstream &ss,
          std::string &s)
{
	s.resize(ss.tellp());
	return s;
}


/* This is a template alternative to nothrow overloads, which
 * allows keeping the function arguments sanitized of the thrownness.
 */

template<class exception_t>
constexpr bool
is_nothrow()
{
	return std::is_same<exception_t, std::nothrow_t>::value;
}

template<class exception_t  = std::nothrow_t,
         class return_t     = bool>
using nothrow_overload = typename std::enable_if<is_nothrow<exception_t>(), return_t>::type;

template<class exception_t,
         class return_t     = void>
using throw_overload = typename std::enable_if<!is_nothrow<exception_t>(), return_t>::type;


//
// Test if type is forward declared or complete
//

template<class T,
         class = void>
struct is_complete
:std::false_type
{};

template<class T>
struct is_complete<T, decltype(void(sizeof(T)))>
:std::true_type
{};


//
// Test if type is a specialization of a template
//

template<class,
         template<class...>
                  class>
struct is_specialization_of
:std::false_type
{};

template<template<class...>
                  class T,
                  class... args>
struct is_specialization_of<T<args...>, T>
:std::true_type
{};



//
// Convenience constexprs for iterators
//

template<class It>
constexpr auto
is_iterator()
{
	return std::is_base_of<typename std::iterator_traits<It>::value_type, It>::value;
}

template<class It>
constexpr auto
is_forward_iterator()
{
	return std::is_base_of<std::forward_iterator_tag, typename std::iterator_traits<It>::iterator_category>::value;
}

template<class It>
constexpr auto
is_input_iterator()
{
	return std::is_base_of<std::forward_iterator_tag, typename std::iterator_traits<It>::iterator_category>::value;
}



// std::next with out_of_range exception
template<class It>
typename std::enable_if<is_forward_iterator<It>() || is_input_iterator<It>(), It>::type
at(It &&start,
   It &&stop,
   ssize_t i)
{
	for(; start != stop; --i, std::advance(start, 1))
		if(!i)
			return start;

	throw std::out_of_range("at(a, b, i): 'i' out of range");
}


//
// Some functors for STL
//

template<class container>
struct keys
{
	auto &operator()(typename container::reference v) const
	{
		return v.first;
	}
};

template<class container>
struct values
{
	auto &operator()(typename container::reference v) const
	{
		return v.second;
	}
};


//
// To collapse pairs of iterators down to a single type
//

template<class T>
struct iterpair
:std::pair<T, T>
{
	using std::pair<T, T>::pair;
};

template<class T>
T &
begin(iterpair<T> &i)
{
	return std::get<0>(i);
}

template<class T>
T &
end(iterpair<T> &i)
{
	return std::get<1>(i);
}

template<class T>
const T &
begin(const iterpair<T> &i)
{
	return std::get<0>(i);
}

template<class T>
const T &
end(const iterpair<T> &i)
{
	return std::get<1>(i);
}

//
// To collapse pairs of iterators down to a single type
// typed by an object with iterator typedefs.
//

template<class T>
using iterators = std::pair<typename T::iterator, typename T::iterator>;

template<class T>
using const_iterators = std::pair<typename T::const_iterator, typename T::const_iterator>;

template<class T>
typename T::iterator
begin(const iterators<T> &i)
{
	return i.first;
}

template<class T>
typename T::iterator
end(const iterators<T> &i)
{
	return i.second;
}

template<class T>
typename T::const_iterator
begin(const const_iterators<T> &ci)
{
	return ci.first;
}

template<class T>
typename T::const_iterator
end(const const_iterators<T> &ci)
{
	return ci.second;
}


///
/// Compile-time comparison of string literals
///
constexpr bool
_constexpr_equal(const char *a,
                 const char *b)
{
	return *a == *b && (*a == '\0' || _constexpr_equal(a + 1, b + 1));
}


inline auto
operator!(const std::string &str)
{
	return str.empty();
}

inline auto
operator!(const std::string_view &str)
{
	return str.empty();
}


//
// Iterator based until() matching std::for_each except the function
// returns a bool to continue rather than void.
//
template<class it_a,
         class it_b,
         class boolean_function>
bool
until(it_a a,
      const it_b &b,
      boolean_function&& f)
{
	for(; a != b; ++a)
		if(!f(*a))
			return false;

	return true;
}


/// Convenience loop to test std::is* on a character sequence
template<int (&test)(int) = std::isprint>
ssize_t
ctype(const char *begin,
      const char *const &end)
{
	size_t i(0);
	for(; begin != end; ++begin, ++i)
		if(!test(static_cast<unsigned char>(*begin)))
			return i;

	return -1;
}


template<class lockable>
struct unlock_guard
{
	lockable &l;

	unlock_guard(lockable &l)
	:l{l}
	{
		l.unlock();
	}

	~unlock_guard() noexcept
	{
		l.lock();
	}
};


template<class T>
constexpr bool
is_bool()
{
	using type = typename std::remove_reference<T>::type;
	return std::is_same<type, bool>::value;
}

template<class T>
constexpr bool
is_number()
{
	using type = typename std::remove_reference<T>::type;
	return std::is_arithmetic<type>::value;
}

template<class T>
constexpr bool
is_floating()
{
	using type = typename std::remove_reference<T>::type;
	return is_number<T>() && std::is_floating_point<type>();
}

template<class T>
constexpr bool
is_integer()
{
	return is_number<T>() && !is_floating<T>();
}

struct is_zero
{
	template<class T>
	typename std::enable_if
	<
		is_bool<T>(),
	bool>::type
	test(const bool &value)
	const
	{
		return !value;
	}

	template<class T>
	typename std::enable_if
	<
		is_integer<T>() &&
		!is_bool<T>(),
	bool>::type
	test(const size_t &value)
	const
	{
		return value == 0;
	}

	template<class T>
	typename std::enable_if
	<
		is_floating<T>(),
	bool>::type
	test(const double &value)
	const
	{
		return !(value > 0.0 || value < 0.0);
	}

	template<class T>
	bool operator()(T&& t)
	const
	{
		return test<T>(std::forward<T>(t));
	}
};


constexpr bool
is_powerof2(const long long v)
{
	return v && !(v & (v - 1LL));
}


} // namespace util
} // namespace ircd
