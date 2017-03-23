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

namespace ircd {
namespace ctx  {

const size_t DEFAULT_STACK_SIZE
{
	64_KiB
};

struct context
{
	enum flags
	{
		POST            = 1U << 0,   // Defers spawn with an ios.post()
		DISPATCH        = 1U << 1,   // (Defers) spawn with an ios.dispatch()
		DETACH          = 1U << 2,   // Context deletes itself; see struct context constructor notes
		INTERRUPTED     = 1U << 3,   // Marked
	};

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
	        std::function<void ()>);

	context(const char *const &name,
	        const size_t &stack_size,
	        std::function<void ()>,
	        const flags & = (flags)0);

	context(const char *const &name,
	        const flags &,
	        std::function<void ()>);

	context(const char *const &name,
	        std::function<void ()>,
	        const flags & = (flags)0);

	context(std::function<void ()>,
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
swap(context &a, context &b)
noexcept
{
	std::swap(a.c, b.c);
}

} // namespace ctx
} // namespace ircd
