// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SPIRIT_PARSE_H

// This file is not part of the IRCd standard include list (stdinc.h) because
// it involves extremely expensive boost headers for creating formal spirit
// grammars. This file is automatically included in the spirit.h group.

namespace ircd {
namespace spirit
__attribute__((visibility("internal")))
{
	template<class rule,
	         class... attr>
	bool parse(const char *&start, const char *const &stop, rule&&, attr&&...);

	template<class parent_error,
	         size_t error_show_max  = 128,
	         class rule,
	         class... attr>
	bool parse(const char *&start, const char *const &stop, rule&&, attr&&...);

	template<class rule,
	         class... attr>
	bool parse(std::nothrow_t, const char *&start, const char *const &stop, rule&&, attr&&...) noexcept;
}}

/// Execute the parse. The start pointer is advanced upon successful execution.
/// Failures must not throw: If the grammar contains any epsilon expressions or
/// callbacks which throw it is UB. This overload exists to force suppression
/// of EH from the base of a complex/opaque rule tree.
template<class rule,
         class... attr>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline bool
ircd::spirit::parse(std::nothrow_t,
                    const char *&start,
                    const char *const &stop,
                    rule&& r,
                    attr&&... a)
noexcept try
{
	return ircd::spirit::parse(start, stop, std::forward<rule>(r), std::forward<attr>(a)...);
}
catch(...)
{
	assert(false);
	__builtin_unreachable();
}

/// Execute the parse. The start pointer is advanced upon successful execution.
/// Failures may throw depending on the grammar. Boost's expectation_failure is
/// translated into our expectation_failure describing the failure.
template<class parent_error,
         size_t error_show_max,
         class rule,
         class... attr>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline bool
ircd::spirit::parse(const char *&start,
                    const char *const &stop,
                    rule&& r,
                    attr&&... a)
try
{
	return ircd::spirit::parse(start, stop, std::forward<rule>(r), std::forward<attr>(a)...);
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure<parent_error>
	{
		e, error_show_max
	};
}

/// Low-level qi::parse entry point. Throws boost's qi::expectation_failure;
/// Try not to allow this exception to escape the calling unit.
template<class rule,
         class... attr>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline bool
ircd::spirit::parse(const char *&start,
                    const char *const &stop,
                    rule&& r,
                    attr&&... a)
{
	return qi::parse(start, stop, std::forward<rule>(r), std::forward<attr>(a)...);
}
