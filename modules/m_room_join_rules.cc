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
	"Matrix m.room.join_rules"
};

static void
_changed_rules(const m::event &event)
{
	const m::user::id &sender
	{
		at<"sender"_>(event)
	};

	if(!my(sender))
		return;

	const m::room::id::buf public_room
	{
		"public", my_host()
	};

	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	send(public_room, sender, "ircd.room", room_id, json::strung{event});
}

const m::hookfn<>
_changed_rules_hookfn
{
	_changed_rules,
	{
		{ "_site",    "vm.notify"          },
		{ "type",     "m.room.join_rules"  },
	}
};
