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
_changed_levels(const m::event &event,
                m::vm::eval &)
{
	log::info
	{
		m::log, "%s changed power_levels in %s [%s]",
		json::get<"sender"_>(event),
		json::get<"room_id"_>(event),
		string_view{event.event_id}
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
