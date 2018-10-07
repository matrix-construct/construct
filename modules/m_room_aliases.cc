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
	"Matrix m.room.aliases"
};

const m::room::id::buf
alias_room_id
{
	"alias", ircd::my_host()
};

const m::room
alias_room
{
	alias_room_id
};

void
_changed_aliases(const m::event &event,
                 m::vm::eval &)
{
	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const json::array &aliases
	{
		at<"content"_>(event).get("aliases")
	};

	for(const json::string &alias_ : aliases)
	{
		const m::room::alias &alias{alias_};
		const auto event_id
		{
			send(alias_room, m::me.user_id, "ircd.alias", alias, json::strung{event})
		};

		log::info
		{
			"Updated aliases of %s by %s in %s [%s] => %s",
			string_view{room_id},
			json::get<"sender"_>(event),
			json::get<"event_id"_>(event),
			string_view{alias},
			string_view{event_id}
		};
	}
}

const m::hookfn<m::vm::eval &>
_changed_aliases_hookfn
{
	_changed_aliases,
	{
		{ "_site",    "vm.effect"       },
		{ "type",     "m.room.aliases"  },
	}
};

void
_can_change_aliases(const m::event &event,
                    m::vm::eval &eval)
{
	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const json::array &aliases
	{
		at<"content"_>(event).get("aliases")
	};

	for(const json::string &alias_ : aliases)
	{
		const m::room::alias &alias{alias_};
		if(at<"origin"_>(event) != alias.host())
			throw m::ACCESS_DENIED
			{
				"Cannot set alias for host '%s' from origin '%s'",
				alias.host(),
				at<"origin"_>(event)
			};
	}
}

const m::hookfn<m::vm::eval &>
_can_change_aliases_hookfn
{
	_can_change_aliases,
	{
		{ "_site",    "vm.eval"         },
		{ "type",     "m.room.aliases"  },
	}
};
