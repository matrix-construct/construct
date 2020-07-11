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
	extern const room::id::buf alias_room_id;
	extern const room alias_room;

	static void auth_room_aliases(const event &, room::auth::hookdata &);
	extern hookfn<room::auth::hookdata &> auth_room_aliases_hookfn;

	static void changed_room_aliases(const event &, vm::eval &);
	extern hookfn<vm::eval &> changed_room_aliases_hookfn;

	extern hookfn<m::vm::eval &> create_alias_room_hookfn;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix m.room.aliases"
};

//
// create the alias room as an effect of !ircd created on bootstrap
//

decltype(ircd::m::create_alias_room_hookfn)
ircd::m::create_alias_room_hookfn
{
	{
		{ "_site",       "vm.effect"      },
		{ "room_id",     "!ircd"          },
		{ "type",        "m.room.create"  },
	},
	[](const m::event &event, m::vm::eval &)
	{
		auto &my
		{
			m::my(at<"origin"_>(event))
		};

		const m::room::id::buf alias_room_id
		{
			"alias", origin(my)
		};

		create(alias_room_id, my.self);
	}
};

//
// an effect of room aliases changed
//

decltype(ircd::m::changed_room_aliases_hookfn)
ircd::m::changed_room_aliases_hookfn
{
	changed_room_aliases,
	{
		{ "_site",    "vm.effect"       },
		{ "type",     "m.room.aliases"  },
	}
};

void
ircd::m::changed_room_aliases(const m::event &event,
                              m::vm::eval &)
try
{
	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const json::array &aliases
	{
		at<"content"_>(event).get("aliases")
	};

	for(const json::string alias : aliases) try
	{
		m::room::aliases::cache::set(alias, room_id);

		log::info
		{
			m::log, "Updated aliases of %s by %s in %s with %s",
			string_view{room_id},
			json::get<"sender"_>(event),
			string_view{event.event_id},
			string_view{alias},
		};
	}
	catch(const std::exception &e)
	{
		log::error
		{
			m::log, "Updating aliases of %s by %s in %s with %s :%s",
			string_view{room_id},
			json::get<"sender"_>(event),
			string_view{event.event_id},
			string_view{alias},
			e.what(),
		};
	}

	if(m::join_rule(room_id, "public"))
		rooms::summary::set(room_id);
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "Updating aliases of %s by %s in %s :%s",
		json::get<"room_id"_>(event),
		json::get<"sender"_>(event),
		string_view{event.event_id},
		e.what(),
	};
}

//
// auth handler
//

decltype(ircd::m::auth_room_aliases_hookfn)
ircd::m::auth_room_aliases_hookfn
{
	auth_room_aliases,
	{
		{ "_site",    "room.auth"       },
		{ "type",     "m.room.aliases"  },
	}
};

void
ircd::m::auth_room_aliases(const event &event,
                           room::auth::hookdata &data)
{
	using FAIL = m::room::auth::FAIL;
	using conforms = m::event::conforms;

	// 4. If type is m.room.aliases:
	assert(json::get<"type"_>(event) == "m.room.aliases");

	// a. If event has no state_key, reject.
	if(empty(json::get<"state_key"_>(event)))
		throw FAIL
		{
			"m.room.aliases event is missing a state_key."
		};

	// b. If sender's domain doesn't matches state_key, reject.
	if(json::get<"state_key"_>(event) != m::user::id(json::get<"sender"_>(event)).host())
		throw FAIL
		{
			"m.room.aliases event state_key is not the sender's domain."
		};

	// c. Otherwise, allow
	data.allow = true;
}
