// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Matrix m.room.message"
};

static void
_message_notify(const m::event &event,
                m::vm::eval &eval)
{
	const auto &body
	{
		unquote(json::get<"content"_>(event).get("body"))
	};

	log::info
	{
		"%s %s said in %s %s :%s%s",
		json::get<"origin"_>(event),
		json::get<"sender"_>(event),
		json::get<"room_id"_>(event),
		json::get<"content"_>(event).get("msgtype"),
		trunc(body, 128),
		size(body) > 128? "..."_sv : string_view{}
	};
}

const m::hookfn<m::vm::eval &>
_message_notify_hookfn
{
	{
		{ "_site",  "vm.notify"       },
		{ "type",   "m.room.message"  },
	},
	_message_notify
};
