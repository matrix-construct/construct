// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::mapi::header
IRCD_MODULE
{
	"Application Services"
};

namespace ircd::m::app
{
	static void handle_event(const m::event &, vm::eval &);

	extern hookfn<vm::eval &> notify_hook;
}

decltype(ircd::m::app::notify_hook)
ircd::m::app::notify_hook
{
	handle_event,
	{
		{ "_site", "vm.notify" },
	}
};

void
ircd::m::app::handle_event(const m::event &event,
                           vm::eval &eval)
try
{
	// Drop internal room traffic
	if(eval.room_internal)
		return;

	// Drop EDU's ???
	if(!event.event_id)
		return;

}
catch(const std::exception &e)
{
	log::error
	{
		log, "Failed to handle for application services :%s",
		e.what(),
	};
}
