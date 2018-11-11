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
#define HAVE_IRCD_CTX_EXCEPTION_HANDLER_H

namespace ircd::ctx::this_ctx
{
	struct exception_handler;
}

/// An instance of exception_handler must be present to allow a context
/// switch inside a catch block. This is due to ABI limitations that stack
/// exceptions with thread-local assumptions and don't expect catch blocks
/// on the same thread to interleave when we switch the stack.
///
/// We first increment the refcount for the caught exception so it remains
/// intuitively accessible for the rest of the catch block. Then the presence
/// of this object makes the ABI believe the catch block has ended.
///
/// The exception cannot then be rethrown. DO NOT RETHROW THE EXCEPTION.
///
struct ircd::ctx::this_ctx::exception_handler
:std::exception_ptr
{
	exception_handler() noexcept;
	exception_handler(exception_handler &&) = delete;
	exception_handler(const exception_handler &) = delete;
	exception_handler &operator=(exception_handler &&) = delete;
	exception_handler &operator=(const exception_handler &) = delete;
};
