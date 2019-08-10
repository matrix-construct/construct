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
	static void changed_canonical_alias(const event &, vm::eval &);

	extern const room alias_room;
	extern const room::id::buf alias_room_id;
	extern hookfn<vm::eval &> changed_canonical_alias_hookfn;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix m.room.canonical_alias"
};

decltype(ircd::m::alias_room_id)
ircd::m::alias_room_id
{
	"alias", ircd::my_host()
};

decltype(ircd::m::alias_room)
ircd::m::alias_room
{
	alias_room_id
};

decltype(ircd::m::changed_canonical_alias_hookfn)
ircd::m::changed_canonical_alias_hookfn
{
	changed_canonical_alias,
	{
		{ "_site",    "vm.effect"               },
		{ "type",     "m.room.canonical_alias"  },
	}
};

void
ircd::m::changed_canonical_alias(const event &event,
                                 vm::eval &)
{
	const room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const m::room room
	{
		room_id, event.event_id
	};

	const json::object &content
	{
		at<"content"_>(event)
	};

	const json::string &content_alias
	{
		content.get("alias")
	};

	const room::alias &alias
	{
		!empty(content_alias)?
			m::room::alias{content_alias}:
			m::room::alias{}
	};

	if(alias)
	{
		if(m::room::aliases::cache::has(alias))
			return;

		m::room::aliases::cache::set(alias, room_id);
		log::info
		{
			m::log, "Canonical alias %s for %s added by %s",
			string_view{alias},
			string_view{room_id},
			json::get<"sender"_>(event),
		};

		return;
	}

	const auto present_event_idx
	{
		room.get(std::nothrow, "m.room.canonical_alias", "")
	};

	if(!present_event_idx)
		return;

	const auto prev_state_idx
	{
		m::room::state::prev(present_event_idx)
	};

	m::get(std::nothrow, prev_state_idx, "content", [&room_id, &event]
	(const json::object &content)
	{
		const json::string &alias
		{
			content["alias"]
		};

		m::room::aliases::cache::del(alias);
		log::info
		{
			m::log, "Canonical alias of %s removed by %s was %s",
			string_view{room_id},
			json::get<"sender"_>(event),
			string_view{alias},
		};
	});
}
