/*
 * charybdis: oh just a little chat server
 * ctx.h: userland context switching (stackful coroutines)
 *
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
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
#define HAVE_IRCD_CTX_CONTEXT_H

namespace ircd::ctx
{
	struct context;

	const size_t DEFAULT_STACK_SIZE
	{
		64_KiB
	};
}

/// Principal interface for a context.
///
/// This object creates, holds and destroys a context with an interface
/// similar to that of std::thread.
///
/// The function passed to the constructor is executed on a new stack. By
/// default this execution will start to occur before this object is even
/// fully constructed. To delay child execution pass the `POST` flag to
/// the constructor; the execution will then be posted to the io_service
/// event queue instead. `DISPATCH` is an alternative, see boost::asio docs.
///
/// When this object goes out of scope the context is interrupted and joined
/// if it has not been already; the current context will wait for this to
/// complete. If the child context does not cooperate the destructor will hang.
/// To prevent this behavior `detach()` the ctx from this handler object; the
/// detached context will free its own resources when finished executing.
///
/// To wait for the child context to finish use `join()`. Calling `interrupt()`
/// will cause an `interrupted` exception to be thrown down the child's stack
/// from the next interruption point; a context switch is an interruption point
/// and so the context will wake up in its exception handler.
///
struct ircd::ctx::context
{
	enum flags
	{
		POST            = 0x01,   ///< Defers spawn with an ios.post()
		DISPATCH        = 0x02,   ///< Defers spawn with an ios.dispatch() (possibly)
		DETACH          = 0x04,   ///< Context deletes itself; see struct context constructor notes

		INTERRUPTED     = 0x10,   ///< (INDICATOR) Marked
	};

	using function = std::function<void ()>;

  private:
	std::unique_ptr<ctx> c;

  public:
	operator const ctx &() const                 { return *c;                                      }
	operator ctx &()                             { return *c;                                      }

	bool operator!() const                       { return !c;                                      }
	operator bool() const                        { return bool(c);                                 }
	bool joined() const                          { return !c || ircd::ctx::finished(*c);           }

	void interrupt()                             { ircd::ctx::interrupt(*c);                       }
	void join();                                 // Blocks the current context until this one finishes
	ctx *detach();                               // other calls undefined after this call

	// Note: Constructing with DETACH flag makes any further use of this object undefined.
	context(const char *const &name,
	        const size_t &stack_size,
	        const flags &,
	        function);

	context(const char *const &name,
	        const size_t &stack_size,
	        function,
	        const flags & = (flags)0);

	context(const char *const &name,
	        const flags &,
	        function);

	context(const char *const &name,
	        function,
	        const flags & = (flags)0);

	context(function,
	        const flags & = (flags)0);

	context() = default;
	context(context &&) = default;
	context(const context &) = delete;
	context &operator=(context &&) = default;
	context &operator=(const context &) = delete;
	~context() noexcept;

	friend void swap(context &a, context &b) noexcept;
};

inline void
ircd::ctx::swap(context &a, context &b)
noexcept
{
	std::swap(a.c, b.c);
}
