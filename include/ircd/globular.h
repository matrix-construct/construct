// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_GLOBULAR_H

namespace ircd
{
	// Globular ('*' and '?') expression utils.
	struct globular_imatch;
	struct globular_iequals;
}

/// Globular equals. This allows either side of the comparison to include '*'
/// and '?' characters and equality of the string expressions will be
/// determined. Case insensitive.
struct ircd::globular_iequals
:boolean
{
	using is_transparent = std::true_type;

	bool operator()(const string_view &a, const string_view &b) const noexcept;

	template<class A,
	         class B>
	globular_iequals(A&& a, B&& b)
	:boolean{operator()(std::forward<A>(a), std::forward<B>(b))}
	{}
};

/// Globular match. Similar to globular_equals but only one side of the
/// comparison is considered to be the expression with '*' and '?' characters.
/// The expression string is passed at construction. The comparison inputs are
/// treated as non-expression strings. This allows for greater optimization
/// than globular_equals. Case insensitive.
struct ircd::globular_imatch
{
	string_view a;

	bool operator()(const string_view &b) const noexcept;

	globular_imatch(const string_view &expr)
	:a{expr}
	{}
};
