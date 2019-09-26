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
	static void changed_history_visibility(const event &, vm::eval &);
	extern hookfn<vm::eval &> changed_history_visibility_hookfn;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix m.room.history_visibility"
};

decltype(ircd::m::changed_history_visibility_hookfn)
ircd::m::changed_history_visibility_hookfn
{
	changed_history_visibility,
	{
		{ "_site",    "vm.effect"                  },
		{ "type",     "m.room.history_visibility"  },
	}
};

void
ircd::m::changed_history_visibility(const event &event,
                                    vm::eval &)
{
	log::info
	{
		log, "Changed visibility of %s to %s by %s => %s",
		json::get<"room_id"_>(event),
		json::get<"content"_>(event).get("history_visibility"),
		json::get<"sender"_>(event),
		string_view{event.event_id},
	};
}
