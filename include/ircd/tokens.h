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
#define HAVE_IRCD_TOKENS_H

//
// String tokenization utils
//
namespace ircd
{
	// Use the closure for best performance. Note that string_view's are not
	// required to be null terminated. Construct an std::string from the view
	// to allocate and copy the token with null termination.
	using token_view = std::function<void (const string_view &)>;
	void tokens(const string_view &str, const char &sep, const token_view &);
	void tokens(const string_view &str, const char *const &sep, const token_view &);
	size_t tokens(const string_view &str, const char &sep, const size_t &limit, const token_view &);
	size_t tokens(const string_view &str, const char *const &sep, const size_t &limit, const token_view &);

	// Copies tokens into your buffer and null terminates strtok() style. Returns BYTES of buf consumed.
	size_t tokens(const string_view &str, const char &sep, const mutable_buffer &buf, const token_view &);
	size_t tokens(const string_view &str, const char *const &sep, const mutable_buffer &buf, const token_view &);

	// Receive token view into iterator range
	template<class it, class sep> it tokens(const string_view &str, const sep &, const it &b, const it &e);

	// Receive token view into array
	template<size_t N, class sep> size_t tokens(const string_view &str, const sep &, string_view (&buf)[N]);
	template<size_t N, class sep> size_t tokens(const string_view &str, const sep &, std::array<string_view, N> &);

	// Receive token view into new container (custom allocator)
	template<template<class, class>
	         class C, //= std::vector,
	         class T = string_view,
	         class A,
	         class sep>
	C<T, A> tokens(A&& allocator, const string_view &str, const sep &);

	// Receive token view into new container
	template<template<class, class>
	         class C, //= std::vector,
	         class T = string_view,
	         class A = std::allocator<T>,
	         class sep>
	C<T, A> tokens(const string_view &str, const sep &);

	// Receive token view into new associative container (custom allocator)
	template<template<class, class, class>
	         class C,
	         class T = string_view,
	         class Comp = std::less<T>,
	         class A,
	         class sep>
	C<T, Comp, A> tokens(A&& allocator, const string_view &str, const sep &);

	// Receive token view into new associative container
	template<template<class, class, class>
	         class C,
	         class T = string_view,
	         class Comp = std::less<T>,
	         class A = std::allocator<T>,
	         class sep>
	C<T, Comp, A> tokens(const string_view &str, const sep &);

	// Convenience to get individual tokens
	size_t token_count(const string_view &str, const char &sep);
	size_t token_count(const string_view &str, const char *const &sep);
	string_view token(const string_view &str, const char &sep, const size_t &at);
	string_view token(const string_view &str, const char *const &sep, const size_t &at);
	string_view token(const string_view &str, const char &sep, const size_t &at, const string_view &def);
	string_view token(const string_view &str, const char *const &sep, const size_t &at, const string_view &def);
	string_view token_last(const string_view &str, const char &sep);
	string_view token_last(const string_view &str, const char *const &sep);
	string_view token_first(const string_view &str, const char &sep);
	string_view token_first(const string_view &str, const char *const &sep);
	string_view tokens_after(const string_view &str, const char &sep, const size_t &at);
	string_view tokens_after(const string_view &str, const char *const &sep, const size_t &at);
	string_view tokens_before(const string_view &str, const char &sep, const size_t &at);
	string_view tokens_before(const string_view &str, const char *const &sep, const size_t &at);
}

template<size_t N,
         class delim>
size_t
ircd::tokens(const string_view &str,
             const delim &sep,
             string_view (&buf)[N])
{
	const auto e(tokens(str, sep, begin(buf), end(buf)));
	return std::distance(begin(buf), e);
}

template<size_t N,
         class delim>
size_t
ircd::tokens(const string_view &str,
             const delim &sep,
             std::array<string_view, N> &buf)
{
	const auto e(tokens(str, sep, begin(buf), end(buf)));
	return std::distance(begin(buf), e);
}

template<class it,
         class delim>
it
ircd::tokens(const string_view &str,
             const delim &sep,
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
         class A,
         class delim>
C<T, Comp, A>
ircd::tokens(const string_view &str,
             const delim &sep)
{
	return tokens<C, T, Comp, A>(A{}, str, sep);
}

template<template<class, class, class>
         class C,
         class T,
         class Comp,
         class A,
         class delim>
C<T, Comp, A>
ircd::tokens(A&& allocator,
             const string_view &str,
             const delim &sep)
{
	C<T, Comp, A> ret(std::forward<A>(allocator));
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
         class A,
         class delim>
C<T, A>
ircd::tokens(const string_view &str,
             const delim &sep)
{
	return tokens<C, T, A>(A{}, str, sep);
}

template<template<class, class>
         class C,
         class T,
         class A,
         class delim>
C<T, A>
ircd::tokens(A&& allocator,
             const string_view &str,
             const delim &sep)
{
	C<T, A> ret(std::forward<A>(allocator));
	tokens(str, sep, [&ret]
	(const string_view &token)
	{
		ret.emplace(ret.end(), token);
	});

	return ret;
}
