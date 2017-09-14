/*
 * charybdis: 21st Century IRC++d
 * util.h: Miscellaneous utilities
 *
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
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
 *
 */

#pragma once
#define HAVE_IRCD_UTIL_H

/// Tools for developers
namespace ircd::util
{
}

namespace ircd        {
inline namespace util {

#define IRCD_EXPCAT(a, b)   a ## b
#define IRCD_CONCAT(a, b)   IRCD_EXPCAT(a, b)
#define IRCD_UNIQUE(a)      IRCD_CONCAT(a, __COUNTER__)


#define IRCD_OVERLOAD(NAME) \
    static constexpr struct NAME##_t {} NAME {};

#define IRCD_USING_OVERLOAD(ALIAS, ORIGIN) \
    static constexpr const auto &ALIAS{ORIGIN}


#define IRCD_WEAK_TYPEDEF(TYPE, NAME)                       \
struct NAME                                                 \
:TYPE                                                       \
{                                                           \
    using TYPE::TYPE;                                       \
};

#define IRCD_STRONG_TYPEDEF(TYPE, NAME)                       \
struct NAME                                                   \
{                                                             \
    TYPE val;                                                 \
                                                              \
    explicit operator const TYPE &() const  { return val;  }  \
    explicit operator TYPE &()              { return val;  }  \
};

#define IRCD_WEAK_T(TYPE) \
    IRCD_WEAK_TYPEDEF(TYPE, IRCD_UNIQUE(weak_t))

// ex: using foo_t = IRCD_STRONG_T(int)
#define IRCD_STRONG_T(TYPE) \
    IRCD_STRONG_TYPEDEF(TYPE, IRCD_UNIQUE(strong_t))

// for complex static initialization (try to avoid this though)
enum class init_priority
{
    FIRST           = 101,
    STD_CONTAINER   = 102,
};

#define IRCD_INIT_PRIORITY(name) \
    __attribute__((init_priority(int(ircd::init_priority::name))))

struct scope
{
	struct nominal;
	struct exceptional;

	const std::function<void ()> func;

	template<class F>
	scope(F &&func): func(std::forward<F>(func)) {}
	scope() = default;
	scope(const scope &) = delete;
	scope &operator=(const scope &) = delete;
	~scope() noexcept
	{
		func();
	}
};

struct scope::nominal
:scope
{
	template<class F>
	nominal(F &&func)
	:scope
	{
		[func(std::forward<F>(func))]
		{
			if(likely(!std::uncaught_exception()))
				func();
		}
	}{}

	nominal() = default;
};

struct scope::exceptional
:scope
{
	template<class F>
	exceptional(F &&func)
	:scope
	{
		[func(std::forward<F>(func))]
		{
			if(unlikely(std::uncaught_exception()))
				func();
		}
	}{}

	exceptional() = default;
};


template<class T>
using custom_ptr = std::unique_ptr<T, std::function<void (T *) noexcept>>;


//
// Iteration of a tuple
//
// for_each(tuple, [](auto&& elem) { ... });

template<size_t i,
         class func,
         class... args>
constexpr
typename std::enable_if<i == std::tuple_size<std::tuple<args...>>::value, void>::type
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
constexpr
typename std::enable_if<(i > 0), bool>::type
runtil(std::tuple<args...> &t,
       func&& f)
{
	return f(std::get<i - 1>(t))? runtil<i - 1>(t, f) : false;
}

template<ssize_t i = -1,
         class func,
         class... args>
constexpr
typename std::enable_if<(i == -1), bool>::type
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
constexpr
typename std::enable_if<(i == -1), bool>::type
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


// For conforming enums include a _NUM_ as the last element,
// then num_of<my_enum>() works
template<class Enum>
constexpr
typename std::underlying_type<Enum>::type
num_of()
{
    return static_cast<typename std::underlying_type<Enum>::type>(Enum::_NUM_);
}

// Iteration of a num_of() conforming enum
template<class Enum>
typename std::enable_if<std::is_enum<Enum>::value, void>::type
for_each(const std::function<void (const Enum &)> &func)
{
    for(size_t i(0); i < num_of<Enum>(); ++i)
        func(static_cast<Enum>(i));
}

/**
 * flag-enum utilities
 *
 * This relaxes the strong typing of enums to allow bitflags with operations on the elements
 * with intuitive behavior.
 *
 * If the project desires absolute guarantees on the strong enum typing then this can be tucked
 * away in some namespace and imported into select scopes instead.
 */

template<class Enum>
constexpr
typename std::enable_if<std::is_enum<Enum>::value, Enum>::type
operator~(const Enum &a)
{
	using enum_t = typename std::underlying_type<Enum>::type;

	return static_cast<Enum>(~static_cast<enum_t>(a));
}

template<class Enum>
constexpr
typename std::enable_if<std::is_enum<Enum>::value, bool>::type
operator!(const Enum &a)
{
	using enum_t = typename std::underlying_type<Enum>::type;

	return !static_cast<enum_t>(a);
}

template<class Enum>
constexpr
typename std::enable_if<std::is_enum<Enum>::value, Enum>::type
operator|(const Enum &a, const Enum &b)
{
	using enum_t = typename std::underlying_type<Enum>::type;

	return static_cast<Enum>(static_cast<enum_t>(a) | static_cast<enum_t>(b));
}

template<class Enum>
constexpr
typename std::enable_if<std::is_enum<Enum>::value, Enum>::type
operator&(const Enum &a, const Enum &b)
{
	using enum_t = typename std::underlying_type<Enum>::type;

	return static_cast<Enum>(static_cast<enum_t>(a) & static_cast<enum_t>(b));
}

template<class Enum>
constexpr
typename std::enable_if<std::is_enum<Enum>::value, Enum>::type
operator^(const Enum &a, const Enum &b)
{
	using enum_t = typename std::underlying_type<Enum>::type;

	return static_cast<Enum>(static_cast<enum_t>(a) ^ static_cast<enum_t>(b));
}

template<class Enum>
constexpr
typename std::enable_if<std::is_enum<Enum>::value, Enum &>::type
operator|=(Enum &a, const Enum &b)
{
	using enum_t = typename std::underlying_type<Enum>::type;

	return (a = (a | b));
}

template<class Enum>
constexpr
typename std::enable_if<std::is_enum<Enum>::value, Enum &>::type
operator&=(Enum &a, const Enum &b)
{
	using enum_t = typename std::underlying_type<Enum>::type;

	return (a = (a & b));
}

template<class Enum>
constexpr
typename std::enable_if<std::is_enum<Enum>::value, Enum &>::type
operator^=(Enum &a, const Enum &b)
{
	using enum_t = typename std::underlying_type<Enum>::type;

	return (a = (a ^ b));
}


inline size_t
size(std::ostream &s)
{
	const auto cur(s.tellp());
	s.seekp(0, std::ios::end);
	const auto ret(s.tellp());
	s.seekp(cur, std::ios::beg);
	return ret;
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


constexpr size_t
hash(const char *const &str,
     const size_t i = 0)
{
	return !str[i]? 7681ULL : (hash(str, i+1) * 33ULL) ^ str[i];
}

inline size_t
hash(const std::string_view &str,
     const size_t i = 0)
{
	return i >= str.size()? 7681ULL : (hash(str, i+1) * 33ULL) ^ str.at(i);
}

inline size_t
hash(const std::string &str,
     const size_t i = 0)
{
	return i >= str.size()? 7681ULL : (hash(str, i+1) * 33ULL) ^ str.at(i);
}

constexpr size_t
hash(const char16_t *const &str,
     const size_t i = 0)
{
	return !str[i]? 7681ULL : (hash(str, i+1) * 33ULL) ^ str[i];
}

inline size_t
hash(const std::u16string &str,
     const size_t i = 0)
{
	return i >= str.size()? 7681ULL : (hash(str, i+1) * 33ULL) ^ str.at(i);
}


/***
 * C++14 user defined literals
 *
 * These are very useful for dealing with space. Simply write 8_MiB and it's
 * as if a macro turned that into (8 * 1024 * 1024) at compile time.
 */

#define IRCD_UNIT_LITERAL_LL(name, morphism)        \
constexpr auto                                      \
operator"" _ ## name(const unsigned long long val)  \
{                                                   \
    return (morphism);                              \
}

#define IRCD_UNIT_LITERAL_LD(name, morphism)        \
constexpr auto                                      \
operator"" _ ## name(const long double val)         \
{                                                   \
    return (morphism);                              \
}

// IEC unit literals
IRCD_UNIT_LITERAL_LL( B,    val                                                              )
IRCD_UNIT_LITERAL_LL( KiB,  val * 1024LL                                                     )
IRCD_UNIT_LITERAL_LL( MiB,  val * 1024LL * 1024LL                                            )
IRCD_UNIT_LITERAL_LL( GiB,  val * 1024LL * 1024LL * 1024LL                                   )
IRCD_UNIT_LITERAL_LL( TiB,  val * 1024LL * 1024LL * 1024LL * 1024LL                          )
IRCD_UNIT_LITERAL_LL( PiB,  val * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL                 )
IRCD_UNIT_LITERAL_LL( EiB,  val * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL        )

IRCD_UNIT_LITERAL_LD( B,    val                                                              )
IRCD_UNIT_LITERAL_LD( KiB,  val * 1024.0L                                                    )
IRCD_UNIT_LITERAL_LD( MiB,  val * 1024.0L * 1024.0L                                          )
IRCD_UNIT_LITERAL_LD( GiB,  val * 1024.0L * 1024.0L * 1024.0L                                )
IRCD_UNIT_LITERAL_LD( TiB,  val * 1024.0L * 1024.0L * 1024.0L * 1024.0L                      )
IRCD_UNIT_LITERAL_LD( PiB,  val * 1024.0L * 1024.0L * 1024.0L * 1024.0L * 1024.0L            )
IRCD_UNIT_LITERAL_LD( EiB,  val * 1024.0L * 1024.0L * 1024.0L * 1024.0L * 1024.0L * 1024.0L  )

// SI unit literals
IRCD_UNIT_LITERAL_LL( KB,   val * 1000LL                                                     )
IRCD_UNIT_LITERAL_LL( MB,   val * 1000LL * 1000LL                                            )
IRCD_UNIT_LITERAL_LL( GB,   val * 1000LL * 1000LL * 1000LL                                   )
IRCD_UNIT_LITERAL_LL( TB,   val * 1000LL * 1000LL * 1000LL * 1000LL                          )
IRCD_UNIT_LITERAL_LL( PB,   val * 1000LL * 1000LL * 1000LL * 1000LL * 1000LL                 )
IRCD_UNIT_LITERAL_LL( EB,   val * 1000LL * 1000LL * 1000LL * 1000LL * 1000LL * 1000LL        )

IRCD_UNIT_LITERAL_LD( KB,   val * 1000.0L                                                    )
IRCD_UNIT_LITERAL_LD( MB,   val * 1000.0L * 1000.0L                                          )
IRCD_UNIT_LITERAL_LD( GB,   val * 1000.0L * 1000.0L * 1000.0L                                )
IRCD_UNIT_LITERAL_LD( TB,   val * 1000.0L * 1000.0L * 1000.0L * 1000.0L                      )
IRCD_UNIT_LITERAL_LD( PB,   val * 1000.0L * 1000.0L * 1000.0L * 1000.0L * 1000.0L            )
IRCD_UNIT_LITERAL_LD( EB,   val * 1000.0L * 1000.0L * 1000.0L * 1000.0L * 1000.0L * 1000.0L  )


/* Output the sizeof a structure at compile time.
 * This stops the compiler with an error (good) containing the size of the target
 * in the message.
 *
 * example: struct foo {}; IRCD_TEST_SIZEOF(foo)
 */

template<size_t SIZE>
struct _TEST_SIZEOF_;

#define IRCD_TEST_SIZEOF(name) \
	ircd::util::_TEST_SIZEOF_<sizeof(name)> _test_;


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
{
};

template<class T>
struct is_complete<T, decltype(void(sizeof(T)))>
:std::true_type
{
};


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
// Error-checking closure for POSIX system calls. Note the usage is
// syscall(read, foo, bar, baz) not a macro like syscall(read(foo, bar, baz));
//
template<class function,
         class... args>
auto
syscall(function&& f,
        args&&... a)
{
	const auto ret(f(a...));
	if(unlikely(long(ret) == -1))
		throw std::system_error(errno, std::system_category());

	return ret;
}


//
// Similar to a va_list, but conveying std-c++ type data acquired from a variadic template
// parameter pack acting as the ...) elipsis. This is used to implement fmt::snprintf(),
// exceptions and logger et al in their respective translation units rather than the header
// files.
//
// Limitations: The choice of a fixed array of N is because std::initializer_list doesn't
// work here and other containers may be heavy in this context. Ideas to improve this are
// welcome.
//
const size_t VA_RTTI_MAX_SIZE = 12;
struct va_rtti
:std::array<std::pair<const void *, const std::type_info *>, VA_RTTI_MAX_SIZE>
{
	using base_type = std::array<value_type, VA_RTTI_MAX_SIZE>;

	static constexpr size_t max_size()
	{
		return std::tuple_size<base_type>();
	}

	size_t argc;
	const size_t &size() const
	{
		return argc;
	}

	template<class... Args>
	va_rtti(Args&&... args)
	:base_type
	{{
		std::make_pair(std::addressof(args), std::addressof(typeid(Args)))...
	}}
	,argc
	{
		sizeof...(args)
	}
	{
		assert(argc <= max_size());
	}
};

static_assert
(
	sizeof(va_rtti) == (va_rtti::max_size() * 16) + 8,
	"va_rtti should be (8 + 8) * N + 8;"
	" where 8 + 8 are the two pointers carrying the argument and its type data;"
	" where N is the max arguments;"
	" where the final + 8 bytes holds the actual number of arguments passed;"
);


//
// To collapse pairs of iterators down to a single type
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


//
// For objects using the pattern of adding their own instance to a container
// in their constructor, storing an iterator as a member, and then removing
// themselves using the iterator in their destructor. It is unsafe to do that.
// Use this instead.
//
template<class container,
         class iterator = typename container::iterator>
struct unique_iterator
{
	container *c;
	iterator it;

	unique_iterator(container &c, iterator it)
	:c{&c}
	,it{std::move(it)}
	{}

	unique_iterator()
	:c{nullptr}
	{}

	unique_iterator(const unique_iterator &) = delete;
	unique_iterator(unique_iterator &&o)
	:c{std::move(o.c)}
	,it{std::move(o.it)}
	{
		o.c = nullptr;
	}

	~unique_iterator() noexcept
	{
		if(c)
			c->erase(it);
	}
};

template<class container>
struct unique_const_iterator
:unique_iterator<container, typename container::const_iterator>
{
	using iterator_type = typename container::const_iterator;
	using unique_iterator<container, iterator_type>::unique_iterator;
};


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


//
// Compile-time comparison of string literals
//
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


} // namespace util
} // namespace ircd
