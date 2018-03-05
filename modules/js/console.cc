// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/js/js.h>

namespace ircd  {
namespace js    {

struct console
:trap
{
	static const char *const source;
	struct module module;

	console();
}
static console;

console::console()
:trap
{
	"__console",
	JSCLASS_HAS_PRIVATE
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

const char *const
console::source
{R"(

	export function critical(msg)      { __console.critical(msg);              }
	export function error(msg)         { __console.error(msg);                 }
	export function warn(msg)          { __console.warn(msg);                  }
	export function notice(msg)        { __console.notice(msg);                }
	export function info(msg)          { __console.info(msg);                  }
	export function debug(msg)         { __console.debug(msg);                 }

	export function cout(msg)          { __console.cout(msg);                  }
	export function log(msg)           { __console.info(msg);                  }

)"};

struct console_critical
:trap::function
{
	value on_call(object::handle obj, value::handle _this, const args &args) override
	{
		ircd::js::log(ircd::log::CRITICAL, "%s", std::string(args[0]));
		return {};
	}

	using trap::function::function;
}
static console_critical{console, "critical"};

struct console_error
:trap::function
{
	value on_call(object::handle obj, value::handle _this, const args &args) override
	{
		ircd::js::log(ircd::log::ERROR, "%s", std::string(args[0]));
		return {};
	}

	using trap::function::function;
}
static console_error{console, "error"};

struct console_warn
:trap::function
{
	value on_call(object::handle obj, value::handle _this, const args &args) override
	{
		ircd::js::log(ircd::log::WARNING, "%s", std::string(args[0]));
		return {};
	}

	using trap::function::function;
}
static console_warn{console, "warn"};

struct console_notice
:trap::function
{
	value on_call(object::handle obj, value::handle _this, const args &args) override
	{
		ircd::js::log(ircd::log::NOTICE, "%s", std::string(args[0]));
		return {};
	}

	using trap::function::function;
}
static console_notice{console, "notice"};

struct console_info
:trap::function
{
	value on_call(object::handle obj, value::handle _this, const args &args) override
	{
		ircd::js::log(ircd::log::INFO, "%s", std::string(args[0]));
		return {};
	}

	using trap::function::function;
}
static console_info{console, "info"};

struct console_debug
:trap::function
{
	value on_call(object::handle obj, value::handle _this, const args &args) override
	{
		ircd::js::log.debug("%s", std::string(args[0]));
		return {};
	}

	using trap::function::function;
}
static console_debug{console, "debug"};

struct console_cout
:trap::function
{
	value on_call(object::handle obj, value::handle _this, const args &args) override
	{
		std::cout << string(args[0]) << std::endl;
		return {};
	}

	using trap::function::function;
}
static console_cout{console, "cout"};

} // namespace js
} // namespace ircd

extern "C" ircd::js::module *
IRCD_JS_MODULE
{
	&ircd::js::console.module
};

ircd::mapi::header
IRCD_MODULE
{
	"Provides simple I/O for debugging similar to that found in web browsers."
};
