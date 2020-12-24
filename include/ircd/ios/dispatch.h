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
#define HAVE_IRCD_IOS_DISPATCH_H

namespace ircd::ios
{
	struct dispatch;

	/// Hard flag to indicate the function is not to be executed during this
	/// epoch, and enqueued instead. This results in asynchronous behavior
	/// from dispatch(), returning immediately to the caller.
	IRCD_OVERLOAD(defer)

	/// Hard flag to indicate the current `ircd::context` will yield until the
	/// function is executed, regardless of the mode of that execution. This
	/// results in synchronous behavior from dispatch().
	IRCD_OVERLOAD(yield)
}

namespace ircd
{
	using ios::dispatch;
}

/// Schedule execution on the core event loop.
struct ircd::ios::dispatch
{
	/// Direct dispatch (main stack only): a handler context switch will be
	/// made but the function will be executed immediately on this stack.
	/// Returns directly after the function has completed.
	dispatch(descriptor &, std::function<void ()>);

	/// Direct dispatch (context stacks only): a context switch will be made
	/// but the function will be executed immediately on this stack. Returns
	/// directly after the function has completed.
	dispatch(descriptor &, yield_t, const std::function<void ()> &);

	/// Queued dispatch: push the function to be executed at a later epoch on
	/// the main stack. Returns immediately.
	dispatch(descriptor &, defer_t, std::function<void ()>);

	/// Queued dispatch (context stacks only): push the function to be executed
	/// at a later epoch on the main stack, while suspending this context.
	/// Returns sometime after the function has completed.
	dispatch(descriptor &, defer_t, yield_t, const std::function<void ()> &);

	/// Courtesy yield (alternative to ctx::yield()). This queues a null
	/// function and suspends this context until its completion. Intended to
	/// allow other contexts to execute before continuing this context.
	dispatch(descriptor &, defer_t, yield_t);
};
