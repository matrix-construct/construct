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

struct assertions
{
	assertions();
}
static assertions;

namespace ircd  {
namespace js    {

struct kernel
:trap
{
	JSPrincipals *principals;
	static const char *const source;
	std::shared_ptr<task> process;

	value on_call(object::handle, value::handle, const args &) override;
	value on_set(object::handle, id::handle, value::handle) override;
	value on_get(object::handle, id::handle, value::handle) override;
	void on_add(object::handle, id::handle, value::handle) override;
	bool on_del(object::handle, id::handle) override;
	bool on_has(object::handle, id::handle) override;
	void on_enu(object::handle) override;
	void on_new(object::handle, object &, const args &) override;
	void on_gc(JSObject *const &) override;

	void main() noexcept;

	kernel();
	~kernel() noexcept;
}
static kernel;

} // namespace js
} // namespace ircd

///////////////////////////////////////////////////////////////////////////////
//
// Kernel source
//

const char *const
ircd::js::kernel::source
{R"(

'use strict';

import * as console from "server.console";
import * as listener from "server.listener";

var ircd =
{

};

ircd.opts =
{

};

ircd.opts.listener =
{
	host: "127.0.0.1",
	port: 8448,
	backlog: 1024,
	ssl_certificate_file: "/home/jason/newcert.pem",
	ssl_private_key_file_pem: "/home/jason/privkey.pem",
	ssl_certificate_chain_file: "/home/jason/newcert.pem",
	ssl_tmp_dh_file: "/home/jason/dh512.pem",
};

let main = async function()
{
	console.debug("IRCd.js Greetings from JavaScript");

	listener.listen(ircd.opts.listener);
};

let fini = function()
{
	console.info("IRCd.js finished");
}

main().then(fini);

)"};
///////////////////////////////////////////////////////////////////////////////

ircd::js::kernel::kernel()
:trap
{
	"[global]",
	JSCLASS_GLOBAL_FLAGS_WITH_SLOTS(1) | JSCLASS_HAS_PRIVATE | JSCLASS_EMULATES_UNDEFINED,
}
,principals
{
	nullptr
}
,process
{
	std::make_shared<task>(source)
}
{
}

ircd::js::kernel::~kernel()
noexcept
{
}

void
ircd::js::kernel::main()
noexcept try
{
	std::map<std::string, ircd::module> modules;

	// These modules host databases and have to be loaded first.
    modules.emplace("root.so"s, "root.so"s);
    modules.emplace("client_account.so"s, "client_account.so"s);
    modules.emplace("client_room.so"s, "client_room.so"s);

	for(const auto &name : mods::available())
		if(startswith(name, "client_"))
			modules.emplace(name, name);

	task::enter(process, [](auto &task)
	{
		task.main();
	});

	printf("main finished\n");
	ctx::wait();

	log.debug("Kernel finished");
}
catch(const ircd::ctx::interrupted &e)
{
	log.debug("Kernel interrupted");
	return;
}
catch(const std::exception &e)
{
	log.critical("Kernel PANIC: %s", e.what());
	std::terminate();
}

void
ircd::js::kernel::on_gc(JSObject *const &obj)
{
	//trap::on_gc(obj);
}

void
ircd::js::kernel::on_new(object::handle ca,
                         object &obj,
                         const args &args)
{
	trap::on_new(ca, obj, args);
}

void
ircd::js::kernel::on_enu(object::handle obj)
{
	if(!JS_EnumerateStandardClasses(*cx, obj))
		throw internal_error("Failed to enumerate standard classes");
}

bool
ircd::js::kernel::on_has(object::handle obj,
                         id::handle id)
{
	return trap::on_has(obj, id);
}

bool
ircd::js::kernel::on_del(object::handle obj,
                         id::handle id)
{
	return trap::on_del(obj, id);
}

void
ircd::js::kernel::on_add(object::handle obj,
                         id::handle id,
                         value::handle val)
{
	trap::on_add(obj, id, val);
}

ircd::js::value
ircd::js::kernel::on_get(object::handle obj,
                         id::handle id,
                         value::handle val)
{
	return trap::on_get(obj, id, val);
}

ircd::js::value
ircd::js::kernel::on_set(object::handle obj,
                         id::handle id,
                         value::handle val)
{
	return trap::on_set(obj, id, val);
}

ircd::js::value
ircd::js::kernel::on_call(object::handle ca,
                          value::handle that,
                          const args &args)
{
	return trap::on_call(ca, that, args);
}

// Unmangled alias to the main task.
extern "C"
void kmain()
{
	ircd::js::kernel.main();
}

assertions::assertions()
{
	using ircd::js::error;

	if(!ircd::js::cx)
		throw error("Kernel cannot find required JS context instance on this thread.");
}

const auto on_load([]
{
	using namespace ircd::js;

});

const auto on_unload([]
{
	using namespace ircd::js;

});

ircd::mapi::header IRCD_MODULE
{
	"IRCd.js kernel - program which helps programs run",
	on_load,
	on_unload
};
