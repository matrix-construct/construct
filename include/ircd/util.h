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


#define IRCD_STRONG_TYPEDEF(TYPE, NAME)                     \
struct NAME                                                 \
{                                                           \
    TYPE val;                                               \
                                                            \
    operator const TYPE &() const   { return val;  }        \
    operator TYPE &()               { return val;  }        \
};


// ex: using foo_t = IRCD_STRONG_T(int)
#define IRCD_STRONG_T(TYPE) \
    IRCD_STRONG_TYPEDEF(TYPE, IRCD_UNIQUE(strong_t))


template<class T>
using custom_ptr = std::unique_ptr<T, std::function<void (T *)>>;


struct scope
{
	const std::function<void ()> func;

	template<class F>
	scope(F &&func)
	:func(std::forward<F>(func))
	{
	}

	~scope()
	{
		func();
	}
};


// For conforming enums include a _NUM_ as the last element,
// then num_of<my_enum>() works
template<class Enum>
constexpr
typename std::underlying_type<Enum>::type
num_of()
{
    return static_cast<typename std::underlying_type<Enum>::type>(Enum::_NUM_);
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


#ifdef BOOST_LEXICAL_CAST_INCLUDED
template<class T = std::string,
         class... Args>
auto lex_cast(Args&&... args)
{
	return boost::lexical_cast<T>(std::forward<Args>(args)...);
}
#endif


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
typename std::enable_if<std::is_enum<Enum>::value, Enum>::type
operator~(const Enum &a)
{
	using enum_t = typename std::underlying_type<Enum>::type;

	return static_cast<Enum>(~static_cast<enum_t>(a));
}

template<class Enum>
typename std::enable_if<std::is_enum<Enum>::value, bool>::type
operator!(const Enum &a)
{
	using enum_t = typename std::underlying_type<Enum>::type;

	return !static_cast<enum_t>(a);
}

template<class Enum>
typename std::enable_if<std::is_enum<Enum>::value, Enum>::type
operator|(const Enum &a, const Enum &b)
{
	using enum_t = typename std::underlying_type<Enum>::type;

	return static_cast<Enum>(static_cast<enum_t>(a) | static_cast<enum_t>(b));
}

template<class Enum>
typename std::enable_if<std::is_enum<Enum>::value, Enum>::type
operator&(const Enum &a, const Enum &b)
{
	using enum_t = typename std::underlying_type<Enum>::type;

	return static_cast<Enum>(static_cast<enum_t>(a) & static_cast<enum_t>(b));
}

template<class Enum>
typename std::enable_if<std::is_enum<Enum>::value, Enum>::type
operator^(const Enum &a, const Enum &b)
{
	using enum_t = typename std::underlying_type<Enum>::type;

	return static_cast<Enum>(static_cast<enum_t>(a) ^ static_cast<enum_t>(b));
}

template<class Enum>
typename std::enable_if<std::is_enum<Enum>::value, Enum &>::type
operator|=(Enum &a, const Enum &b)
{
	using enum_t = typename std::underlying_type<Enum>::type;

	return (a = (a | b));
}

template<class Enum>
typename std::enable_if<std::is_enum<Enum>::value, Enum &>::type
operator&=(Enum &a, const Enum &b)
{
	using enum_t = typename std::underlying_type<Enum>::type;

	return (a = (a & b));
}

template<class Enum>
typename std::enable_if<std::is_enum<Enum>::value, Enum &>::type
operator^=(Enum &a, const Enum &b)
{
	using enum_t = typename std::underlying_type<Enum>::type;

	return (a = (a ^ b));
}

}        // namespace util
}        // namespace ircd
#endif   // __cplusplus
