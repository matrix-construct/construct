// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static void room_tombstone_effect_handler(const event &, vm::eval &);
	extern hookfn<vm::eval &> room_tombstone_effect_hook;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix m.room.tombstone"
};

decltype(ircd::m::room_tombstone_effect_hook)
ircd::m::room_tombstone_effect_hook
{
	room_tombstone_effect_handler,
	{
		{ "_site",  "vm.effect"        },
		{ "type",   "m.room.tombstone" },
	}
};

void
ircd::m::room_tombstone_effect_handler(const m::event &event,
                                       m::vm::eval &)
{
	assert(at<"type"_>(event) == "m.room.tombstone");

	const m::room room
	{
		at<"room_id"_>(event)
	};

	if(!creator(room, at<"sender"_>(event)))
		return;

	const m::room::aliases aliases
	{
		room
	};

	aliases.for_each([](const auto &room_alias)
	{
		// Invalidate the alias cache immediately so we don't have to wait a TTL
		m::room::aliases::cache::del(room_alias);
		return true;
	});
}
