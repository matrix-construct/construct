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

#ifdef __cplusplus

namespace ircd        {
inline namespace util {


#define IRCD_EXPCAT(a, b)   a ## b
#define IRCD_CONCAT(a, b)   IRCD_EXPCAT(a, b)
#define IRCD_UNIQUE(a)      IRCD_CONCAT(a, __COUNTER__)


#define IRCD_OVERLOAD(NAME)             \
    struct NAME##_t {};                 \
    static constexpr NAME##_t NAME {};


#define IRCD_WEAK_TYPEDEF(TYPE, NAME)                       \
struct NAME                                                 \
:TYPE                                                       \
{                                                           \
    using TYPE::TYPE;                                       \
};

#define IRCD_STRONG_TYPEDEF(TYPE, NAME)                     \
struct NAME                                                 \
{                                                           \
    TYPE val;                                               \
                                                            \
    operator const TYPE &() const   { return val;  }        \
    operator TYPE &()               { return val;  }        \
};

#define IRCD_WEAK_T(TYPE) \
    IRCD_WEAK_TYPEDEF(TYPE, IRCD_UNIQUE(weak_t))

// ex: using foo_t = IRCD_STRONG_T(int)
#define IRCD_STRONG_T(TYPE) \
    IRCD_STRONG_TYPEDEF(TYPE, IRCD_UNIQUE(strong_t))


template<class T>
using custom_ptr = std::unique_ptr<T, std::function<void (T *) noexcept>>;


struct scope
{
	const std::function<void ()> func;

	template<class F> scope(F &&func);
	scope(const scope &) = delete;
	~scope() noexcept;
};

template<class F>
scope::scope(F &&func)
:func(std::forward<F>(func))
{
}

inline
scope::~scope()
noexcept
{
	func();
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


struct case_insensitive_less
{
	bool operator()(const std::string &a, const std::string &b) const
	{
		return std::lexicographical_compare(begin(a), end(a), begin(b), end(b), []
		(const char &a, const char &b)
		{
			return tolower(a) < tolower(b);
		});
	}
};


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


inline std::pair<time_t, int32_t>
microtime()
{
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	return { tv.tv_sec, tv.tv_usec };
}

inline ssize_t
microtime(char *const &buf,
          const size_t &size)
{
	const auto mt(microtime());
	return snprintf(buf, size, "%zd.%06d", mt.first, mt.second);
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

inline auto
operator!(const std::string &str)
{
	return str.empty();
}


constexpr size_t
hash(const char *const &str,
     const size_t i = 0)
{
	return !str[i]? 7681ULL : (hash(str, i+1) * 33ULL) ^ str[i];
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

#define UNIT_LITERAL_LL(name, morphism)             \
constexpr auto                                      \
operator"" _ ## name(const unsigned long long val)  \
{                                                   \
    return (morphism);                              \
}

#define UNIT_LITERAL_LD(name, morphism)             \
constexpr auto                                      \
operator"" _ ## name(const long double val)         \
{                                                   \
    return (morphism);                              \
}

// IEC unit literals
UNIT_LITERAL_LL( B,    val                                                              )
UNIT_LITERAL_LL( KiB,  val * 1024LL                                                     )
UNIT_LITERAL_LL( MiB,  val * 1024LL * 1024LL                                            )
UNIT_LITERAL_LL( GiB,  val * 1024LL * 1024LL * 1024LL                                   )
UNIT_LITERAL_LL( TiB,  val * 1024LL * 1024LL * 1024LL * 1024LL                          )
UNIT_LITERAL_LL( PiB,  val * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL                 )
UNIT_LITERAL_LL( EiB,  val * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL * 1024LL        )

UNIT_LITERAL_LD( B,    val                                                              )
UNIT_LITERAL_LD( KiB,  val * 1024.0L                                                    )
UNIT_LITERAL_LD( MiB,  val * 1024.0L * 1024.0L                                          )
UNIT_LITERAL_LD( GiB,  val * 1024.0L * 1024.0L * 1024.0L                                )
UNIT_LITERAL_LD( TiB,  val * 1024.0L * 1024.0L * 1024.0L * 1024.0L                      )
UNIT_LITERAL_LD( PiB,  val * 1024.0L * 1024.0L * 1024.0L * 1024.0L * 1024.0L            )
UNIT_LITERAL_LD( EiB,  val * 1024.0L * 1024.0L * 1024.0L * 1024.0L * 1024.0L * 1024.0L  )

// SI unit literals
UNIT_LITERAL_LL( KB,   val * 1000LL                                                     )
UNIT_LITERAL_LL( MB,   val * 1000LL * 1000LL                                            )
UNIT_LITERAL_LL( GB,   val * 1000LL * 1000LL * 1000LL                                   )
UNIT_LITERAL_LL( TB,   val * 1000LL * 1000LL * 1000LL * 1000LL                          )
UNIT_LITERAL_LL( PB,   val * 1000LL * 1000LL * 1000LL * 1000LL * 1000LL                 )
UNIT_LITERAL_LL( EB,   val * 1000LL * 1000LL * 1000LL * 1000LL * 1000LL * 1000LL        )

UNIT_LITERAL_LD( KB,   val * 1000.0L                                                    )
UNIT_LITERAL_LD( MB,   val * 1000.0L * 1000.0L                                          )
UNIT_LITERAL_LD( GB,   val * 1000.0L * 1000.0L * 1000.0L                                )
UNIT_LITERAL_LD( TB,   val * 1000.0L * 1000.0L * 1000.0L * 1000.0L                      )
UNIT_LITERAL_LD( PB,   val * 1000.0L * 1000.0L * 1000.0L * 1000.0L * 1000.0L            )
UNIT_LITERAL_LD( EB,   val * 1000.0L * 1000.0L * 1000.0L * 1000.0L * 1000.0L * 1000.0L  )


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


}        // namespace util
}        // namespace ircd
#endif   // __cplusplus
