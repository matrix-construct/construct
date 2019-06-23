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
#define HAVE_IRCD_CTX_STACK_USAGE_ASSERTION_H

namespace ircd::ctx {
inline namespace this_ctx
{
	struct stack_usage_assertion;

	size_t stack_at_here() __attribute__((noinline));
}}

/// An instance of stack_usage_assertion is placed on a ctx stack where one
/// wants to test the stack usage at both construction and destruction points
/// to ensure it is less than the value set in ctx::prof::settings which is
/// generally some engineering safety factor of 2-3 etc. This should not be
/// entirely relied upon except during debug builds, however we may try to
/// provide an optimized build mode enabling these to account for any possible
/// differences in the stack between the environments.
///
struct ircd::ctx::this_ctx::stack_usage_assertion
{
	#ifndef NDEBUG
	stack_usage_assertion();
	~stack_usage_assertion() noexcept;
	#endif
};
