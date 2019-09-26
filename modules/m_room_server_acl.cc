// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::mapi::header
IRCD_MODULE
{
	"Matrix Room Server Access Control List"
};

//
// vm hookfn's
//

namespace ircd::m
{
	static void on_changed_room_server_acl(const m::event &, m::vm::eval &);
	static void on_check_room_server_acl(const m::event &, m::vm::eval &);

	extern m::hookfn<m::vm::eval &> changed_room_server_acl;
	extern m::hookfn<m::vm::eval &> check_room_server_acl;
}

decltype(ircd::m::check_room_server_acl)
ircd::m::check_room_server_acl
{
	on_check_room_server_acl,
	{
		{ "_site", "vm.access" },
	}
};

void
ircd::m::on_check_room_server_acl(const event &event,
                                  vm::eval &)
{
	if(!m::room::server_acl::enable_write)
		return;

	if(!m::room::server_acl::check(at<"room_id"_>(event), at<"origin"_>(event)))
		throw m::ACCESS_DENIED
		{
			"Server '%s' denied by room %s access control list.",
			at<"origin"_>(event),
			at<"room_id"_>(event),
		};
}

decltype(ircd::m::changed_room_server_acl)
ircd::m::changed_room_server_acl
{
	on_changed_room_server_acl,
	{
		{ "_site",    "vm.notify"          },
		{ "type",     "m.room.server_acl"  },
	}
};

void
ircd::m::on_changed_room_server_acl(const event &event,
                                    vm::eval &)
{
	log::info
	{
		m::log, "%s changed server access control list in %s [%s]",
		json::get<"sender"_>(event),
		json::get<"room_id"_>(event),
		string_view{event.event_id},
    };
}
