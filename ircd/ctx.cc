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

#include <ircd/ctx_ctx.h>

using namespace ircd;

/******************************************************************************
 * ctx.h
 */
__thread ctx::ctx *ctx::current;

std::chrono::microseconds
ctx::wait()
{
	const auto rem(wait(std::chrono::microseconds::max()));
	return std::chrono::microseconds::max() - rem;
}

std::chrono::microseconds
ctx::wait(const std::chrono::microseconds &duration)
{
	assert(current);
	auto &c(cur());

	c.alarm.expires_from_now(duration);
	c.wait(); // now you're yielding with portals

	// return remaining duration.
	// this is > 0 if notified or interrupted
	// this is unchanged if a note prevented any wait at all
	const auto ret(c.alarm.expires_from_now());
	return std::chrono::duration_cast<std::chrono::microseconds>(ret);
}

void
ctx::wait()
{
	auto &c(cur());
	c.alarm.expires_at(std::chrono::steady_clock::time_point::max());
	c.wait(); // now you're yielding with portals
}

ircd::ctx::context::context(const size_t &stack_sz,
                            std::function<void ()> func,
                            const flags &flags)
:c{std::make_unique<ctx>(stack_sz, ircd::ios)}
{
	auto spawn([stack_sz, c(c.get()), func(std::move(func))]
	{
		auto bound(std::bind(&ctx::operator(), c, ph::_1, std::move(func)));
		const boost::coroutines::attributes attrs
		{
			stack_sz,
			boost::coroutines::stack_unwind
		};

		boost::asio::spawn(*ios, std::move(bound), attrs);
	});

	// The current context must be reasserted if spawn returns here
	const scope recurrent([current(ircd::ctx::current)]
	{
		ircd::ctx::current = current;
	});

	if(flags & DEFER_POST)
		ios->post(std::move(spawn));
	else if(flags & DEFER_DISPATCH)
		ios->dispatch(std::move(spawn));
	else
		spawn();
}

ircd::ctx::context::context(std::function<void ()> func,
                            const flags &flags)
:context
{
	DEFAULT_STACK_SIZE,
	std::move(func),
	flags
}
{
}

ircd::ctx::context::~context()
noexcept
{
	if(!c)
		return;

	// Can't join to bare metal, only from within another context.
	if(!current)
		return;

	join();
}

void
ircd::ctx::context::join()
{
	if(joined())
		return;

	assert(!c->adjoindre);
	c->adjoindre = &cur();
	wait();
	c.reset(nullptr);
}

ircd::ctx::ctx *
ircd::ctx::context::detach()
{
	return c.release();
}

bool
ircd::ctx::notify(ctx &ctx)
{
	return ctx.note();
}

bool
ircd::ctx::started(const ctx &ctx)
{
	return ctx.started();
}

const int64_t &
ircd::ctx::notes(const ctx &ctx)
{
	return ctx.notes;
}

void
ircd::ctx::free(const ctx *const ptr)
{
	delete ptr;
}

/******************************************************************************
 * ctx_ctx.h
 */
ctx::ctx::ctx(const size_t &stack_max,
              boost::asio::io_service *const &ios)
:alarm{*ios}
,yc{nullptr}
,stack_base{0}
,stack_max{stack_max}
,notes{1}
,adjoindre{nullptr}
{
}

void
ctx::ctx::operator()(boost::asio::yield_context yc,
                     const std::function<void ()> func)
noexcept
{
	this->yc = &yc;
	notes = 1;
	stack_base = uintptr_t(__builtin_frame_address(0));
	ircd::ctx::current = this;

	// If anything is done to `this' after func() or in atexit, func() cannot
	// delete its own context, which is a worthy enough convenience to preserve.
	const scope atexit([]
	{
		ircd::ctx::current = nullptr;
		this->yc = nullptr;

		if(adjoindre)
			notify(*adjoindre);

		if(flags & SELF_DESTRUCT)
			delete this;
	});

	if(likely(bool(func)))
		func();
}

size_t
ctx::stack_usage_here(const ctx &ctx)
{
	return ctx.stack_base - uintptr_t(__builtin_frame_address(0));
}
