// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static void room_message_info(const event &, vm::eval &);
	extern hookfn<vm::eval &> room_message_info_hook;
	extern log::log room_message_log;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix m.room.message"
};

decltype(ircd::m::room_message_log)
ircd::m::room_message_log
{
	"m.message"
};

decltype(ircd::m::room_message_info_hook)
ircd::m::room_message_info_hook
{
	room_message_info,
	{
		{ "_site",  "vm.notify"       },
		{ "type",   "m.room.message"  },
	}
};

void
ircd::m::room_message_info(const event &event,
                           vm::eval &eval)
{
	const m::room::message msg
	{
		json::get<"content"_>(event)
	};

	const auto &body
	{
		msg.body()
	};

	const auto &msgtype
	{
		json::get<"msgtype"_>(msg)
	};

	log::info
	{
		room_message_log, "%s said %s in %s %s :%s%s",
		json::get<"sender"_>(event),
		string_view{event.event_id},
		json::get<"room_id"_>(event),
		msgtype,
		trunc(body, 128),
		size(body) > 128? "..."_sv : string_view{}
	};
}
