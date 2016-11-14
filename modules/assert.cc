/*
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

#include <ircd/js/js.h>

namespace ircd  {
namespace js    {

struct assert_
:trap
{
	struct ok;

	value on_call(object::handle, value::handle, const args &) override;
	using trap::trap;
}
static assert_{"assert"};

struct assert_::ok
:trap::function
{
	value on_call(object::handle, value::handle, const args &) override;
	using trap::function::function;
}
static ok{assert_, "ok"};

value
assert_::on_call(object::handle callee,
                 value::handle thatv,
                 const args &args)
{
	return call("ok", callee, args);
}

value
assert_::ok::on_call(object::handle,
                     value::handle val,
                     const args &args)
{
	if(!bool(args[0]))
	{
		string message
		{
			args.has(1)? string(args[1]) : string("failed")
		};

		throw jserror("AssertionError: %s", message.c_str());
	}

	return {};
}

} // namespace js
} // namespace ircd

using namespace ircd;

mapi::header IRCD_MODULE
{
	"Provides a set of assertion tests in the js environment",
};
