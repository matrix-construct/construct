// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

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
