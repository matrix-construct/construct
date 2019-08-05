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
	static void auth_room_create(const event &, event::auth::hookdata &);
	extern hookfn<event::auth::hookdata &> auth_room_create_hookfn;

	static void created_room(const event &, vm::eval &);
	extern hookfn<vm::eval &> created_room_hookfn;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix m.room.create"
};

//
// an effect of room created
//

decltype(ircd::m::created_room_hookfn)
ircd::m::created_room_hookfn
{
	created_room,
	{
		{ "_site",    "vm.effect"     },
		{ "type",     "m.room.create" },
	}
};

void
ircd::m::created_room(const m::event &event,
                      m::vm::eval &)
try
{
	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const string_view &local
	{
		room_id.localname()
	};

	if(local != "users") //TODO: circ dep
		send(m::my_room, at<"sender"_>(event), "ircd.room", room_id, json::object{});

	log::debug
	{
		m::log, "Creation of room %s by %s (%s)",
		string_view{room_id},
		at<"sender"_>(event),
		string_view{event.event_id},
	};
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "Effect of creating room %s with %s by %s :%s",
		json::get<"room_id"_>(event),
		string_view{event.event_id},
		json::get<"sender"_>(event),
		e.what()
	};
}

//
// auth handler
//

decltype(ircd::m::auth_room_create_hookfn)
ircd::m::auth_room_create_hookfn
{
	auth_room_create,
	{
		{ "_site",    "event.auth"     },
		{ "type",     "m.room.create"  },
	}
};

void
ircd::m::auth_room_create(const event &event,
                          event::auth::hookdata &data)
{
	using FAIL = m::event::auth::FAIL;
	using conforms = m::event::conforms;

	// 1. If type is m.room.create:
	assert(json::get<"type"_>(event) == "m.room.create");

	// a. If it has any previous events, reject.
	if(count(data.prev) || !empty(data.auth_events))
		throw FAIL
		{
			"m.room.create has previous events."
		};

	// b. If the domain of the room_id does not match the domain of the
	// sender, reject.
	if(conforms(event).has(conforms::MISMATCH_CREATE_SENDER))
		throw FAIL
		{
			"m.room.create room_id domain does not match sender domain."
		};

	// c. If content.room_version is present and is not a recognised
	// version, reject.
	if(json::get<"content"_>(event).has("room_version"))
	{
		const json::string &claim_version
		{
			json::get<"content"_>(event).get("room_version", "1")
		};

		const string_view &id_version
		{
			event.event_id.version()
		};

		if(claim_version == "3")
		{
			if(id_version != "3")
				throw FAIL
				{
					"m.room.create room_version not 3"
				};
		}
		else if(claim_version == "4")
		{
			if(id_version != "4")
				throw FAIL
				{
					"m.room.create room_version not 4"
				};
		}
		else if(claim_version == "1" || claim_version == "2")
		{
			//if(id_version != "1")
			//	return "m.room.create room_version not 1";
		}
		else if(claim_version != "1" && claim_version != "2")
		{
			if(id_version != "4")
				throw FAIL
				{
					"m.room.create room_version not 4"
				};
		}
		else assert(0);
	}

	// d. If content has no creator field, reject.
	if(empty(json::get<"content"_>(event).get("creator")))
		throw FAIL
		{
			"m.room.create content.creator is missing."
		};

	// e. Otherwise, allow.
	data.allow = true;
}
