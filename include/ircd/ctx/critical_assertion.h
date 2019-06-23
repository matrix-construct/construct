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
#define HAVE_IRCD_CTX_CRITICAL_ASSERTION_H

namespace ircd::ctx {
inline namespace this_ctx
{
	struct critical_assertion;         // Assert no yielding for a section
}}

/// An instance of critical_assertion detects an attempt to context switch.
///
/// For when the developer specifically does not want any yielding in a
/// section or anywhere up the stack from it. This device does not prevent
/// a switch and may carry no meaning outside of debug-mode compilation. It is
/// good practice to use this device even when it appears obvious the
/// section's callgraph has no chance of yielding: code changes, and everything
/// up the graph can change without taking notice of your section.
///
class ircd::ctx::this_ctx::critical_assertion
{
	#ifndef NDEBUG
	bool theirs;

  public:
	critical_assertion();
	~critical_assertion() noexcept;
	#endif
};
