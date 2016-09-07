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

#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/spawn.hpp>

namespace ircd {
namespace ctx  {

struct ctx
{
	boost::asio::steady_timer alarm;             // 64B
	boost::asio::yield_context *yc;
	uintptr_t stack_base;
	size_t stack_max;
	int64_t notes;                        // norm: 0 = asleep; 1 = awake; inc by others; dec by self
	ctx *adjoindre;

	bool started() const;
	bool wait();
	void wake();
	bool note();

	void operator()(boost::asio::yield_context, const std::function<void ()>) noexcept;

	ctx(const size_t &stack_max                = DEFAULT_STACK_SIZE,
	    boost::asio::io_service *const &ios    = ircd::ios);

	ctx(ctx &&) noexcept = delete;
	ctx(const ctx &) = delete;
};

struct continuation
{
	ctx *self;

	operator const boost::asio::yield_context &() const;
	operator boost::asio::yield_context &();

	continuation(ctx *const &self = ircd::ctx::current);
	~continuation() noexcept;
};

size_t stack_usage_here(const ctx &) __attribute__((noinline));

inline
continuation::continuation(ctx *const &self)
:self{self}
{
	assert(self != nullptr);
	assert(self->notes <= 1);
	ircd::ctx::current = nullptr;
}

inline
continuation::~continuation()
noexcept
{
	ircd::ctx::current = self;
	self->notes = 1;
}

inline
continuation::operator boost::asio::yield_context &()
{
	return *self->yc;
}

inline
continuation::operator const boost::asio::yield_context &()
const
{
	return *self->yc;
}

inline bool
ctx::note()
{
	if(notes++ > 0)
		return false;

	wake();
	return true;
}

inline void
ctx::wake()
try
{
	strand.dispatch([this]
	{
		alarm.cancel();
	});
}
catch(const boost::system::system_error &e)
{
	ircd::log::error("ctx::wake(%p): %s", this, e.what());
}

inline bool
ctx::wait()
{
	if(--notes > 0)
		return false;

	boost::system::error_code ec;
	alarm.async_wait(boost::asio::yield_context(continuation(this))[ec]);

	assert(ec == boost::system::errc::operation_canceled ||
	       ec == boost::system::errc::success);

	// notes = 1; set by continuation dtor on wakeup
	return true;
}

inline bool
ctx::started()
const
{
	return bool(yc);
}

} // namespace ctx

using ctx::continuation;
using yield = boost::asio::yield_context;

} // namespace ircd
