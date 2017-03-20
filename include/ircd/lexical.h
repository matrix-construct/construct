/*
 *  charybdis: an advanced ircd.
 *  inline/stringops.h: inlined string operations used in a few places
 *
 *  Copyright (C) 2005-2016 Charybdis Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

#pragma once
#define HAVE_IRCD_LEXICAL_H

namespace ircd {

//
// Lexical conversions
//
IRCD_EXCEPTION(ircd::error, bad_lex_cast)

template<class T> bool try_lex_cast(const string_view &);
template<> bool try_lex_cast<long double>(const string_view &);
template<> bool try_lex_cast<double>(const string_view &);
template<> bool try_lex_cast<ulong>(const string_view &);
template<> bool try_lex_cast<long>(const string_view &);
template<> bool try_lex_cast<uint>(const string_view &);
template<> bool try_lex_cast<int>(const string_view &);
template<> bool try_lex_cast<ushort>(const string_view &);
template<> bool try_lex_cast<short>(const string_view &);
template<> bool try_lex_cast<uint8_t>(const string_view &);
template<> bool try_lex_cast<int8_t>(const string_view &);
template<> bool try_lex_cast<bool>(const string_view &);

template<class T> T lex_cast(const string_view &);
template<> long double lex_cast(const string_view &);
template<> double lex_cast(const string_view &);
template<> ulong lex_cast(const string_view &);
template<> long lex_cast(const string_view &);
template<> uint lex_cast(const string_view &);
template<> int lex_cast(const string_view &);
template<> ushort lex_cast(const string_view &);
template<> short lex_cast(const string_view &);
template<> uint8_t lex_cast(const string_view &);
template<> int8_t lex_cast(const string_view &);
template<> bool lex_cast(const string_view &);

// User supplied destination buffer
template<class T> string_view lex_cast(T, char *const &buf, const size_t &max);
template<> string_view lex_cast(long double, char *const &buf, const size_t &max);
template<> string_view lex_cast(double, char *const &buf, const size_t &max);
template<> string_view lex_cast(ulong, char *const &buf, const size_t &max);
template<> string_view lex_cast(long, char *const &buf, const size_t &max);
template<> string_view lex_cast(uint, char *const &buf, const size_t &max);
template<> string_view lex_cast(int, char *const &buf, const size_t &max);
template<> string_view lex_cast(ushort, char *const &buf, const size_t &max);
template<> string_view lex_cast(short, char *const &buf, const size_t &max);
template<> string_view lex_cast(uint8_t, char *const &buf, const size_t &max);
template<> string_view lex_cast(int8_t, char *const &buf, const size_t &max);
template<> string_view lex_cast(bool, char *const &buf, const size_t &max);

// Circular static thread_local buffer
const size_t LEX_CAST_BUFS {8};
template<class T> string_view lex_cast(const T &t);

//
// String tokenization.
//

// Use the closure for best performance. Note that string_view's
// are not required to be null terminated. Construct an std::string from the view to allocate
// and copy the token with null termination.
using token_view = std::function<void (const string_view &)>;
void tokens(const string_view &str, const char *const &sep, const token_view &);
size_t tokens(const string_view &str, const char *const &sep, const size_t &limit, const token_view &);

// Copies tokens into your buffer and null terminates strtok() style. Returns BYTES of buf consumed.
size_t tokens(const string_view &str, const char *const &sep, char *const &buf, const size_t &max, const token_view &);

// Receive token view into iterator range
template<class it> it tokens(const string_view &str, const char *const &sep, const it &b, const it &e);

// Receive token view into array
template<size_t N> size_t tokens(const string_view &str, const char *const &sep, string_view (&buf)[N]);
template<size_t N> size_t tokens(const string_view &str, const char *const &sep, std::array<string_view, N> &);

// Receive token view into new container (custom allocator)
template<template<class, class>
         class C = std::vector,
         class T = string_view,
         class A>
C<T, A> tokens(A allocator, const string_view &str, const char *const &sep);

// Receive token view into new container
template<template<class, class>
         class C = std::vector,
         class T = string_view,
         class A = std::allocator<T>>
C<T, A> tokens(const string_view &str, const char *const &sep);

// Receive token view into new associative container (custom allocator)
template<template<class, class, class>
         class C,
         class T = string_view,
         class Comp = std::less<T>,
         class A>
C<T, Comp, A> tokens(A allocator, const string_view &str, const char *const &sep);

// Receive token view into new associative container
template<template<class, class, class>
         class C,
         class T = string_view,
         class Comp = std::less<T>,
         class A = std::allocator<T>>
C<T, Comp, A> tokens(const string_view &str, const char *const &sep);

// Convenience to get individual tokens
size_t tokens_count(const string_view &str, const char *const &sep);
string_view token(const string_view &str, const char *const &sep, const size_t &at);
string_view token_last(const string_view &str, const char *const &sep);
string_view token_first(const string_view &str, const char *const &sep);
string_view tokens_after(const string_view &str, const char *const &sep, const size_t &at);

//
// Misc utils
//

// Simple case insensitive comparison convenience utils
bool iequals(const string_view &a, const string_view &b);
bool iless(const string_view &a, const string_view &b);

// Vintage
size_t strlcpy(char *const &dest, const char *const &src, const size_t &bufmax);
size_t strlcat(char *const &dest, const char *const &src, const size_t &bufmax);

// Legacy
char *strip_colour(char *string);
char *strip_unprintable(char *string);
char *reconstruct_parv(int parc, const char **parv);

string_view chomp(const string_view &str, const string_view &c = " "s);
std::pair<string_view, string_view> split(const string_view &str, const char &delim = ' ');
std::pair<string_view, string_view> split(const string_view &str, const string_view &delim);
std::pair<string_view, string_view> rsplit(const string_view &str, const char &delim = ' ');
std::pair<string_view, string_view> rsplit(const string_view &str, const string_view &delim);
string_view between(const string_view &str, const string_view &a = "("s, const string_view &b = ")"s);
bool endswith(const string_view &str, const string_view &val);
bool endswith(const string_view &str, const char &val);
template<class It> bool endswith_any(const string_view &str, const It &begin, const It &end);
bool startswith(const string_view &str, const string_view &val);
bool startswith(const string_view &str, const char &val);
string_view unquote(string_view str);

} // namespace ircd


inline ircd::string_view
ircd::unquote(string_view str)
{
	if(startswith(str, '"'))
		str = { str.data() + 1, str.data() + str.size() };

	if(endswith(str, '"'))
		str = { str.data(), str.data() + str.size() - 1 };

	return str;
}

inline bool
ircd::startswith(const string_view &str,
                 const char &val)
{
	return !str.empty() && str[0] == val;
}

inline bool
ircd::startswith(const string_view &str,
                 const string_view &val)
{
	const auto pos(str.find(val, 0));
	return pos == 0;
}

template<class It>
bool
ircd::endswith_any(const string_view &str,
                   const It &begin,
                   const It &end)
{
	return std::any_of(begin, end, [&str](const auto &val)
	{
		return endswith(str, val);
	});
}

inline bool
ircd::endswith(const string_view &str,
               const char &val)
{
	return !str.empty() && str[str.size()-1] == val;
}

inline bool
ircd::endswith(const string_view &str,
               const string_view &val)
{
	const auto vlen(std::min(str.size(), val.size()));
	const auto pos(str.find(val, vlen));
	return pos == str.size() - vlen;
}

inline ircd::string_view
ircd::between(const string_view &str,
              const string_view &a,
              const string_view &b)
{
	return split(split(str, a).second, b).first;
}

inline std::pair<ircd::string_view, ircd::string_view>
ircd::rsplit(const string_view &str,
             const string_view &delim)
{
	const auto pos(str.find_last_of(delim));
	if(pos == string_view::npos) return
	{
		string_view{},
		str
	};
	else return
	{
		str.substr(0, pos),
		str.substr(pos + delim.size())
	};
}

inline std::pair<ircd::string_view, ircd::string_view>
ircd::rsplit(const string_view &str,
             const char &delim)
{
	const auto pos(str.find_last_of(delim));
	if(pos == string_view::npos) return
	{
		string_view{},
		str
	};
	else return
	{
		str.substr(0, pos),
		str.substr(pos + 1)
	};
}

inline std::pair<ircd::string_view, ircd::string_view>
ircd::split(const string_view &str,
            const string_view &delim)
{
	const auto pos(str.find(delim));
	if(pos == string_view::npos) return
	{
		str,
		string_view{}
	};
	else return
	{
		str.substr(0, pos),
		str.substr(pos + delim.size())
	};
}

inline std::pair<ircd::string_view, ircd::string_view>
ircd::split(const string_view &str,
            const char &delim)
{
	const auto pos(str.find(delim));
	if(pos == string_view::npos) return
	{
		str,
		string_view{}
	};
	else return
	{
		str.substr(0, pos),
		str.substr(pos + 1)
	};
}

inline ircd::string_view
ircd::chomp(const string_view &str,
            const string_view &c)
{
	const auto pos(str.find_last_not_of(c));
	return pos != string_view::npos? string_view{str.substr(0, pos + 1)} : str;
}

inline
ircd::params::params(const string_view &in,
                     const char *const &sep,
	                 const std::initializer_list<const char *> &names)
:in{in}
,sep{sep}
,names{names}
{
}

template<class T>
T
ircd::params::at(const size_t &i,
                 const T &def)
const try
{
	return tokens_count(in, sep) > i? at<T>(i) : def;
}
catch(const bad_lex_cast &e)
{
	throw invalid("parameter #%zu <%s>", i, name(i));
}

template<class T>
T
ircd::params::at(const size_t &i)
const try
{
	return lex_cast<T>(at(i));
}
catch(const bad_lex_cast &e)
{
	throw invalid("parameter #%zu <%s>", i, name(i));
}

inline ircd::string_view
ircd::params::at(const size_t &i)
const try
{
	return token(in, sep, i);
}
catch(const std::out_of_range &e)
{
	throw missing("required parameter #%zu <%s>", i, name(i));
}

inline ircd::string_view
ircd::params::operator[](const size_t &i)
const
{
	return tokens_count(in, sep) > i? token(in, sep, i) : string_view{};
}

inline const char *
ircd::params::name(const size_t &i)
const
{
	return names.size() > i? *std::next(begin(names), i) : "<unnamed>";
}

template<size_t N>
size_t
ircd::tokens(const string_view &str,
             const char *const &sep,
             string_view (&buf)[N])
{
	const auto e(tokens(str, sep, begin(buf), end(buf)));
	return std::distance(begin(buf), e);
}

template<size_t N>
size_t
ircd::tokens(const string_view &str,
             const char *const &sep,
             std::array<string_view, N> &buf)
{
	const auto e(tokens(str, sep, begin(buf), end(buf)));
	return std::distance(begin(buf), e);
}

template<class it>
it
ircd::tokens(const string_view &str,
             const char *const &sep,
             const it &b,
             const it &e)
{
	it pos(b);
	tokens(str, sep, std::distance(b, e), [&pos]
	(const string_view &token)
	{
		*pos = token;
		++pos;
	});

	return pos;
}

template<template<class, class, class>
         class C,
         class T,
         class Comp,
         class A>
C<T, Comp, A>
ircd::tokens(const string_view &str,
             const char *const &sep)
{
	A allocator;
	return tokens<C, T, Comp, A>(allocator, str, sep);
}

template<template<class, class, class>
         class C,
         class T,
         class Comp,
         class A>
C<T, Comp, A>
ircd::tokens(A allocator,
             const string_view &str,
             const char *const &sep)
{
	C<T, Comp, A> ret(allocator);
	tokens(str, sep, [&ret]
	(const string_view &token)
	{
		ret.emplace(ret.end(), token);
	});

	return ret;
}

template<template<class, class>
         class C,
         class T,
         class A>
C<T, A>
ircd::tokens(const string_view &str,
             const char *const &sep)
{
	A allocator;
	return tokens<C, T, A>(allocator, str, sep);
}

template<template<class, class>
         class C,
         class T,
         class A>
C<T, A>
ircd::tokens(A allocator,
             const string_view &str,
             const char *const &sep)
{
	C<T, A> ret(allocator);
	tokens(str, sep, [&ret]
	(const string_view &token)
	{
		ret.emplace(ret.end(), token);
	});

	return ret;
}

inline size_t
#ifndef HAVE_STRLCPY
ircd::strlcpy(char *const &dest,
              const char *const &src,
              const size_t &max)
{
	if(!max)
		return 0;

	const size_t ret(strnlen(src, max));
	const size_t len(ret >= max? max - 1 : ret);
	memcpy(dest, src, len);
	dest[len] = '\0';

	return ret;
}
#else
ircd::strlcpy(char *const &dest,
              const char *const &src,
              const size_t &max)
{
	return ::strlcpy(dest, src, max);
}
#endif

inline size_t
#ifndef HAVE_STRLCAT
ircd::strlcat(char *const &dest,
              const char *const &src,
              const size_t &max)
{
	if(!max)
		return 0;

	const ssize_t dsize(strnlen(dest, max));
	const ssize_t ssize(strnlen(src, max));
	const ssize_t ret(dsize + ssize);
	const ssize_t remain(max - dsize);
	const ssize_t cpsz(ssize >= remain? remain - 1 : ssize);
	char *const ptr(dest + dsize);
	memcpy(ptr, src, cpsz);
	ptr[cpsz] = '\0';
	return ret;
}
#else
ircd::strlcat(char *const &dest,
              const char *const &src,
              const size_t &max)
{
	return ::strlcat(dest, src, max);
}
#endif

inline bool
ircd::iless(const string_view &a,
            const string_view &b)
{
	return std::lexicographical_compare(begin(a), end(a), begin(b), end(b), []
	(const char &a, const char &b)
	{
		return tolower(a) < tolower(b);
	});
}

inline bool
ircd::iequals(const string_view &a,
              const string_view &b)
{
	return std::equal(begin(a), end(a), begin(b), end(b), []
	(const char &a, const char &b)
	{
		return tolower(a) == tolower(b);
	});
}

template<class T>
ircd::string_view
ircd::lex_cast(const T &t)
{
	return lex_cast<T>(t, nullptr, 0);
}
