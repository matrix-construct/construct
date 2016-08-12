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

inline namespace ircd {
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


struct case_insensitive_less
:std::binary_function<std::string, std::string, bool>
{
    bool operator() (const std::string &lhs, const std::string &rhs) const
    {
        return rb_strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
    }
};


#ifdef HAVE_IRCD_MATCH_H
struct case_mapped_less
:std::binary_function<std::string, std::string, bool>
{
    bool operator() (const std::string &lhs, const std::string &rhs) const
    {
        return irccmp(lhs.c_str(), rhs.c_str()) < 0;
    }
};
#endif


}        // namespace util
}        // namespace ircd
#endif   // __cplusplus
