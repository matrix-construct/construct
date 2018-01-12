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
// Misc string() participants
//

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


} // namespace util
} // namespace ircd
