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

#include <ircd/listen.h>
#include <ircd/js/js.h>

namespace ircd {
namespace js   {

struct listener
:js::trap
{
	struct listen;

	static const char *const source;
	struct module module;

	listener();
}
static listener;

struct listener::listen
:trap::function
{
	ircd::listener *l;

	value on_call(object::handle obj, value::handle _this, const args &args) override
	{
		l = new ircd::listener(std::string(args[0]));
		return {};
	}

	using trap::function::function;
}
static listener_listen
{
	listener, "listen"
};

} // namespace js
} // namespace ircd

///////////////////////////////////////////////////////////////////////////////
//
// Listener source
//

const char *const
ircd::js::listener::source
{R"(

import * as console from "server.console";

export function listen(opts)
{
	__listener.listen(JSON.stringify(opts));
}

)"};
///////////////////////////////////////////////////////////////////////////////

ircd::js::listener::listener()
:js::trap
{
	"__listener",
}
,module
{
	JS::CompileOptions(*cx),
	locale::char16::conv(source),
	this,
	true
}
{
}

extern "C" ircd::js::module *
IRCD_JS_MODULE
{
	&ircd::js::listener.module
};

ircd::mapi::header
IRCD_MODULE
{
	"Network listener socket support for servers"
};
