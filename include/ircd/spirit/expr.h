// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SPIRIT_EXPR_H

namespace ircd::spirit
{
	template<class T>
	struct [[clang::nodebug]] expr;
}

/// Expression storage wrapper. This could also be called `named_expression`.
/// This allows spirit expressions to be instantiated in global variables
/// along with a simple name string (and potentially other metadata). Why not
/// just use a rule, you say? The rule templates perform type erasure on the
/// original expression by constructing function objects which are prototyped
/// to the rule such that the expression is lost behind a call. Compilers are
/// able to optimize this like it was never even there, but only when all rules
/// are constructed in a local stack frame. **Clang is unable to compose rules
/// defined at global scope without indirect calls**.
///
/// Declarations of this object are blessed with C++17 template deduction. At
/// global scope, compose expressions first before constructing a single rule
/// right before parsing. !!! Do not allow global scope rules to refer to each
/// other, even at function scope. !!! Do not allow exprs to refer to rules,
/// ever. !!! Only allow global scope rules to refer to exprs, and exprs to
/// other exprs.
///
/// Rules constructed at function scope usually generate fully inline parsers.
/// Rules constructed at global scope usually generate parsers behind a direct
/// call. Everything else generates a soup of handler functions behind a web
/// of indirect calls.
///
template<class T>
struct [[gnu::visibility("internal"), clang::internal_linkage]]
ircd::spirit::expr
:boost::proto::result_of::deep_copy<T>::type
{
	const char *const &name;

	constexpr expr(T &&, const char *const &name);
	constexpr expr(const T &, const char *const &name);
};

template<class T>
constexpr
ircd::spirit::expr<T>::expr(const T &e,
                            const char *const &name)
:boost::proto::result_of::deep_copy<T>::type
{
	boost::proto::deep_copy(e)
}
,name
{
	name
}
{}

template<class T>
constexpr
ircd::spirit::expr<T>::expr(T &&e,
                            const char *const &name)
:boost::proto::result_of::deep_copy<T>::type
{
	boost::proto::deep_copy(std::move(e))
}
,name
{
	name
}
{}
