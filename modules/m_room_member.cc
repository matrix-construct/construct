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
	"Matrix m.room.member"
};

static void
affect_user_room(const m::event &event,
                 m::vm::eval &eval)
{
	const auto &room_id
	{
		at<"room_id"_>(event)
	};

	const auto &sender
	{
		at<"sender"_>(event)
	};

	const m::user::id &subject
	{
		at<"state_key"_>(event)
	};

	//TODO: ABA / TXN
	if(!exists(subject))
		create(subject);

	//TODO: ABA / TXN
	m::user::room user_room{subject};
	send(user_room, sender, "ircd.member", room_id, at<"content"_>(event));
}

const m::hookfn<m::vm::eval &>
affect_user_room_hookfn
{
	{
		{ "_site",          "vm.effect"     },
		{ "type",           "m.room.member" },
	},
	affect_user_room
};

static void
_join_room(const m::event &event,
           m::vm::eval &eval)
{

}

const m::hookfn<m::vm::eval &>
_join_room_hookfn
{
	{
		{ "_site",          "vm.effect"     },
		{ "type",           "m.room.member" },
		{ "membership",     "join"          },
	},
	_join_room
};

using invite_foreign_proto = m::event::id::buf (const m::event &);
mods::import<invite_foreign_proto>
invite__foreign
{
	"client_rooms", "invite__foreign"
};

static void
invite_foreign(const m::event &event,
               m::vm::eval &eval)
{
	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const m::user::id &target
	{
		at<"state_key"_>(event)
	};

	const auto target_host
	{
		target.host()
	};

	if(m::my_host(target_host))
		return;

	const m::room::origins origins
	{
		room_id
	};

	if(origins.has(target_host))
		return;

	const auto eid
	{
		invite__foreign(event)
	};

}

const m::hookfn<m::vm::eval &>
invite_foreign_hookfn
{
	{
		{ "_site",          "vm.issue"      },
		{ "type",           "m.room.member" },
		{ "membership",     "invite"        },
	},
	invite_foreign
};
