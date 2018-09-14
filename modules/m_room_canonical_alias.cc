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
	"Matrix m.room.canonical_alias"
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
_changed_canonical_alias(const m::event &event)
{
	const m::room::alias &alias
	{
		unquote(at<"content"_>(event).at("alias"))
	};

	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const auto event_id
	{
		send(alias_room, m::me.user_id, "ircd.alias", alias, json::strung{event})
	};

	log::info
	{
		"Changed canonical alias of %s to %s by %s with %s => %s",
		string_view{room_id},
		string_view{alias},
		json::get<"sender"_>(event),
		json::get<"event_id"_>(event),
		string_view{event_id}
	};
}

const m::hookfn<>
_changed_canonical_alias_hookfn
{
	_changed_canonical_alias,
	{
		{ "_site",    "vm.notify"               },
		{ "type",     "m.room.canonical_alias"  },
	}
};

void
_can_change_canonical_alias(const m::event &event,
                            m::vm::eval &eval)
{
	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const m::room::alias &alias
	{
		unquote(at<"content"_>(event).at("alias"))
	};

	const m::room room
	{
		room_id
	};

	bool has_alias{false};
	room.get(std::nothrow, "m.room.aliases", alias.host(), [&alias, &has_alias]
	(const m::event &event)
	{
		const json::array &aliases
		{
			json::get<"content"_>(event).at("aliases")
		};

		for(auto it(begin(aliases)); it != end(aliases) && !has_alias; ++it)
			if(unquote(*it) == alias)
				has_alias = true;
	});

	if(!has_alias)
		throw m::ACCESS_DENIED
		{
			"Cannot set canonical alias '%s' because it is not an alias in '%s'",
			string_view{alias},
			string_view{room_id}
		};
}

const m::hookfn<m::vm::eval &>
_can_change_canonical_alias_hookfn
{
	_can_change_canonical_alias,
	{
		{ "_site",    "vm.eval"                 },
		{ "type",     "m.room.canonical_alias"  },
	}
};
