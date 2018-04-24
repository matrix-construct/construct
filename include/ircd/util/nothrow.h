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
#define HAVE_IRCD_UTIL_NOTHROW_H

//
// Template nothrow suite
//

namespace ircd {
namespace util {

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

} // namespace util
} // namespace ircd
