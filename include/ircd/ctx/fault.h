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
#define HAVE_IRCD_CTX_FAULT_H

namespace ircd::ctx
{
	IRCD_EXCEPTION(error, aborted)
	IRCD_EXCEPTION(aborted, unhandled_fault)

	template<class... args> struct fault;
}

// Faults add the notion of recoverable exceptions. C++ exceptions are not, as
// they destruct the stack and then clobber everything with the catch branch.
// A fault is an error handling device alternative to throwing an exception;
// Hitting a fault may stop the context until the fault is serviced to continue
// or a real exception is thrown to abort the context.
//
// A compelling example is std::bad_alloc, or an out of memory condition. A
// fault allows other contexts to free up their resources after which the
// faulty context can continue without having to unwind the work it's already
// made progress on to try again.
//
// Faults begin with the cost of a function call to a handler at the point of
// the fault. The handler's template specifies the argument list so the fault
// can safely observe or modify your data. The call to fault has no return
// value. If it returns the fault has been successfully serviced.
//
// Fault handlers must return true to default the faulty context. Handlers are
// also responsible for detecting if they are executing with an active
// exception which makes returning false considered a DOUBLE FAULT. This may
// lead to program termination because it's basically throwing an exception from
// a destructor (and can be useful proper behavior).
//
template<class... args>
struct ircd::ctx::fault
{
	using handler = std::function<bool (args&&...)>;

	handler h;

	virtual bool handle(args&&... a)
	{
		if(unlikely(!h))
			throw unhandled_fault{};

		return h(std::forward<args>(a)...);
	}

	void operator()(args&&... a)
	{
		if(!handle(std::forward<args>(a)...))
			throw aborted{};
	}

	fault(handler h)
	:h{std::move(h)}
	{}
};
