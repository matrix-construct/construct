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

ircd::ctx::context::context(const size_t &stack_size,
                            const function &func)
:c{std::make_unique<ctx>(stack_size)}
{
	static const boost::coroutines::attributes attrs
	{
		stack_size,
		boost::coroutines::stack_unwind
	};

	const auto bound(std::bind(&ctx::operator(), c.get(), ph::_1, func));
	boost::asio::spawn(c->strand, bound, attrs);
}

ircd::ctx::context::context(const function &func)
:context
{
	DEFAULT_STACK_SIZE,
	func
}
{
}

ircd::ctx::context::~context()
noexcept
{
	join();
}

void
ircd::ctx::context::join()
{

}

ircd::ctx::context &
ircd::ctx::context::swap(context &other)
noexcept
{
	std::swap(c, other.c);
	return *this;
}

ircd::ctx::ctx *
ircd::ctx::context::detach()
{
	return c.release();
}

void
ircd::ctx::swap(context &a, context &b)
noexcept
{
	a.swap(b);
}

/******************************************************************************
 * ctx_ctx.h
 */
ctx::ctx::ctx(const size_t &stack_size,
              boost::asio::io_service *const &ios)
:strand{*ios}
,alarm{*ios}
,stack_size{stack_size}
,stack_base{nullptr}
,yc{nullptr}
{
}

ctx::ctx::~ctx()
noexcept
{
}

void
ctx::ctx::operator()(boost::asio::yield_context yc,
                     const std::function<void ()> func)
{
	this->stack_base = reinterpret_cast<const uint8_t *>(__builtin_frame_address(0));
	this->yc = &yc;
	ircd::ctx::current = this;

	if(likely(bool(func)))
		func();
}
