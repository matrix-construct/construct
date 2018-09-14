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
	"Matrix m.room.create"
};

static void
_can_create_room(const m::event &event,
                 m::vm::eval &eval)
{
	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const m::user::id &sender
	{
		at<"sender"_>(event)
	};

	if(room_id.host() != sender.host())
		throw m::ACCESS_DENIED
		{
			"sender must be on the room_id's host"
		};
}

const m::hookfn<m::vm::eval &>
_can_create_room_hookfn
{
	_can_create_room,
	{
		{ "_site",    "vm.commit"     },
		{ "type",     "m.room.create" },
	}
};

static void
_created_room(const m::event &event)
{
	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const string_view &local
	{
		room_id.localname()
	};

	if(local != "users") //TODO: circ dep
		send(m::my_room, at<"sender"_>(event), "ircd.room", room_id, json::object{});
}

const m::hookfn<>
_created_room_hookfn
{
	_created_room,
	{
		{ "_site",    "vm.notify"     },
		{ "type",     "m.room.create" },
	}
};
