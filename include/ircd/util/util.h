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

#include "typography.h"
#include "unit_literal.h"
#include "unwind.h"
#include "reentrance.h"
#include "enum.h"
#include "syscall.h"
#include "va_rtti.h"
#include "unique_iterator.h"
#include "instance_list.h"
#include "bswap.h"
#include "tuple.h"
#include "timer.h"
#include "life_guard.h"

// Unsorted section
namespace ircd {
namespace util {

/// A standard unique_ptr but accepting an std::function for T as its custom
/// deleter. This reduces the boilerplate burden on declaring the right
/// unique_ptr template for custom deleters every single time.
///
template<class T>
using custom_ptr = std::unique_ptr<T, std::function<void (T *) noexcept>>;

//
// Misc size() participants.
//

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

//
// Misc data() participants
//

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

//
// String generating patterns
//

/// This is the ubiquitous ircd::string() template serving as the "toString()"
/// for the project. Particpating types that want to have a string(T)
/// returning an std::string must friend an operator<<(std::ostream &, T);
/// this is primarily for debug strings, not meant for performance or service.
///
template<class T>
auto
string(const T &s)
{
	std::stringstream ss;
	ss << s;
	return ss.str();
}

inline auto
string(const char *const &buf,
       const size_t &size)
{
	return std::string{buf, size};
}

inline auto
string(const uint8_t *const &buf,
       const size_t &size)
{
	return string(reinterpret_cast<const char *>(buf), size);
}

/// Close over the common pattern to write directly into a post-C++11 standard
/// string through the data() member requiring a const_cast. Closure returns
/// the final size of the data written into the buffer.
inline auto
string(const size_t &size,
       const std::function<size_t (const mutable_buffer &)> &closure)
{
	std::string ret(size, char{});
	const mutable_buffer buf
	{
		const_cast<char *>(ret.data()), ret.size()
	};

	const size_t consumed
	{
		closure(buf)
	};

	assert(consumed <= buffer::size(buf));
	data(buf)[consumed] = '\0';
	ret.resize(consumed);
	return ret;
}

/// Close over the common pattern to write directly into a post-C++11 standard
/// string through the data() member requiring a const_cast. Closure returns
/// a view of the data actually written to the buffer.
inline auto
string(const size_t &size,
       const std::function<string_view (const mutable_buffer &)> &closure)
{
	return string(size, [&closure]
	(const mutable_buffer &buffer)
	{
		return ircd::size(closure(buffer));
	});
}

//
// Misc bang participants
//

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
// stringstream buffer set macros
//

template<class stringstream>
stringstream &
pubsetbuf(stringstream &ss,
          const mutable_buffer &buf)
{
	ss.rdbuf()->pubsetbuf(data(buf), size(buf));
	return ss;
}

template<class stringstream>
stringstream &
pubsetbuf(stringstream &ss,
          std::string &s)
{
	auto *const &data
	{
		const_cast<char *>(s.data())
	};

	ss.rdbuf()->pubsetbuf(data, s.size());
	return ss;
}

template<class stringstream>
stringstream &
pubsetbuf(stringstream &ss,
          std::string &s,
          const size_t &size)
{
	s.resize(size, char{});
	return pubsetbuf(ss, s);
}

template<class stringstream>
stringstream &
resizebuf(stringstream &ss,
          std::string &s)
{
	s.resize(ss.tellp());
	return ss;
}

/// buf has to match the rdbuf you gave the stringstream
template<class stringstream>
string_view
view(stringstream &ss,
     const const_buffer &buf)
{
	assert(size_t(ss.tellp()) <= size(buf));
	ss.flush();
	ss.rdbuf()->pubsync();
	return
	{
		data(buf), size_t(ss.tellp())
	};
}

//
// Template nothrow suite
//

/// Test for template geworfenheit
///
template<class exception_t>
constexpr bool
is_nothrow()
{
	return std::is_same<exception_t, std::nothrow_t>::value;
}

/// This is a template alternative to nothrow overloads, which
/// allows keeping the function arguments sanitized of the thrownness.
///
template<class exception_t  = std::nothrow_t,
         class return_t     = bool>
using nothrow_overload = typename std::enable_if<is_nothrow<exception_t>(), return_t>::type;

/// Inverse of the nothrow_overload template
///
template<class exception_t,
         class return_t     = void>
using throw_overload = typename std::enable_if<!is_nothrow<exception_t>(), return_t>::type;

/// Like std::next() but with out_of_range exception
///
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

/// Compile-time comparison of string literals
///
constexpr bool
_constexpr_equal(const char *a,
                 const char *b)
{
	return *a == *b && (*a == '\0' || _constexpr_equal(a + 1, b + 1));
}

/// Iterator based until() matching std::for_each except the function
/// returns a bool to continue rather than void.
///
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

/// Inverse of std::lock_guard<>
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

constexpr bool
is_powerof2(const long long v)
{
	return v && !(v & (v - 1LL));
}

//
// Transform to pointer utils
//

/// Transform input sequence values to pointers in the output sequence
/// using two input iterators [begin, end] and one output iterator [begin]
template<class input_begin,
         class input_end,
         class output_begin>
auto
pointers(input_begin&& ib,
         const input_end &ie,
         output_begin&& ob)
{
	return std::transform(ib, ie, ob, []
	(auto&& value)
	{
		return std::addressof(value);
	});
}

template<class input_container,
         class output_container>
auto
pointers(input_container&& ic,
         output_container &oc)
{
	return pointers(begin(ic), end(ic), begin(oc));
}


/// Get what() from exception_ptr
///
inline ircd::string_view
what(const std::exception_ptr &eptr)
try
{
	if(likely(eptr))
		std::rethrow_exception(eptr);

	return {};
}
catch(const std::exception &e)
{
	return e.what();
}
catch(...)
{
	return {};
}


} // namespace util
} // namespace ircd
