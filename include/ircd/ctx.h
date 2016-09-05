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
#define HAVE_IRCD_CTX_H

#ifdef __cplusplus
namespace ircd {
namespace ctx  {

IRCD_EXCEPTION(ircd::error, error)
IRCD_EXCEPTION(error, interrupted)

const auto DEFAULT_STACK_SIZE = 64_KiB;

struct ctx;
class context
{
	using ptr = std::unique_ptr<ctx>;
	using function = std::function<void ()>;

	ptr c;

  public:
	void join();
	ctx *detach();
	context &swap(context &) noexcept;

	context(const size_t &stack_size, const function &);
	context(const function &);
	context(context &&) noexcept = default;
	context(const context &) = delete;
	~context() noexcept;
};

extern __thread struct ctx *current;

void swap(context &a, context &b) noexcept;

inline
ctx &cur()
{
	assert(current);
	return *current;
}

}      // namespace ctx

using ctx::cur;
using ctx::context;

}      // namespace ircd
#endif // __cplusplus
