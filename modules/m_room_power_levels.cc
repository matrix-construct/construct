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
	"Matrix m.room.power_levels"
};

static void
_has_power(const m::event &event,
           m::vm::eval &)
{
	const auto &room_id
	{
		json::get<"room_id"_>(event)
	};

	const auto &event_id
	{
		json::get<"event_id"_>(event)
	};

	if(!room_id || !event_id) // Not evaluated here
		return;

	const auto &type
	{
		at<"type"_>(event)
	};

	if(type == "m.room.create") // Checked by _can_create_room
		return;

	const m::room room
	{
		room_id //TODO: at event
	};

	const m::user::id &sender
	{
		at<"sender"_>(event)
	};

	//TODO: ircd.presence
	if(my(event) && my(room) && !my(sender))
		return;

	const m::room::power power
	{
		room
	};

	const auto &state_key
	{
		json::get<"state_key"_>(event)
	};

	if(!power(sender, "events", type, state_key))
		log::error //TODO: throw
		{
			"Power violation %s in %s for %s %s,%s",
			string_view{sender},
			string_view{room_id},
			string_view{event_id},
			type,
			state_key
		};
}

const m::hookfn<m::vm::eval &>
_has_power_hookfn
{
	_has_power,
	{
		{ "_site", "vm.eval" },
	}
};

static void
_can_change_levels(const m::event &event,
                   m::vm::eval &)
{
	//TODO: abstract event check against existing power levels should
	//TODO: cover this?
}

const m::hookfn<m::vm::eval &>
_can_change_levels_hookfn
{
	_can_change_levels,
	{
		{ "_site",    "vm.eval"              },
		{ "type",     "m.room.power_levels"  },
	}
};

static void
_changed_levels(const m::event &event,
                m::vm::eval &)
{
	log::info
	{
		"%s changed power_levels in %s [%s]",
		json::get<"sender"_>(event),
		json::get<"room_id"_>(event),
		json::get<"event_id"_>(event)
    };
}

const m::hookfn<m::vm::eval &>
_changed_levels_hookfn
{
	_changed_levels,
	{
		{ "_site",    "vm.notify"            },
		{ "type",     "m.room.power_levels"  },
	}
};
