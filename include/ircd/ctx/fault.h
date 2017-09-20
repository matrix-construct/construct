/*
 *  Copyright (C) 2016 Charybdis Development Team
 *  Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
