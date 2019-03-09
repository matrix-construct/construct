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
_can_join_room(const m::event &event,
               m::vm::eval &eval)
{
	const m::room room
	{
		at<"room_id"_>(event) //TODO: state at prev/auth whatever
	};

	const m::user::id &sender
	{
		at<"sender"_>(event)
	};

	// Fetch the user's membership from the room's state.
	char membership_buf[32];
	const string_view membership
	{
		room.membership(membership_buf, sender)
	};

	// If the user is already in the "join" state no further checks here.
	if(membership == "join")
		return;

	// If the user is in the "ban" state here no further checks either.
	if(membership == "ban")
		throw m::FORBIDDEN
		{
			"%s is banned from room %s",
			string_view{sender},
			string_view{room.room_id}
		};

	// Fetch the join_rules from the room's state.
	char join_rule_buf[32];
	const string_view join_rule
	{
		room.join_rule(mutable_buffer(join_rule_buf))
	};

	// If the room is "public" no further checks here.
	if(join_rule == "public")
		return;

	if(join_rule != "invite")
		log::dwarning
		{
			m::log, "Unsupported join_rule '%s' for room %s. Defaulting to 'invite'.",
			join_rule,
			string_view{room.room_id}
		};

	// The room is "invite" and the user is invited; no further checks here.
	if(membership == "invite")
		return;

	// The room is invite and the user is not in the room, either in "leave"
	// state or no membership state (!membership).
	if(membership && membership != "leave")
		log::dwarning
		{
			m::log, "Unsupported membership state '%s' for %s in room %s.",
			membership,
			string_view{sender},
			string_view{room.room_id}
		};

	const m::room::power power
	{
		room
	};

	// The user has power to manipulate an m.room.member state event; note
	// this branch will accept a room creator joining the room right after
	// creation when no power_levels, join_rules, etc exist yet.
	if(power(sender, "events", "m.room.member", sender))
		return;

	throw m::FORBIDDEN
	{
		"%s cannot join room %s",
		string_view{sender},
		string_view{room.room_id}
	};
}

const m::hookfn<m::vm::eval &>
_can_join_room_hookfn
{
	{
		{ "_site",          "vm.eval"       },
		{ "type",           "m.room.member" },
		{ "membership",     "join"          },
	},
	_can_join_room
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
