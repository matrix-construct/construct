// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static void handle_breadcrumb_rooms_focus_out(const event &, vm::eval &, const json::array &);
	static void handle_breadcrumb_rooms_focus_in(const event &, vm::eval &, const json::array &);
	static void handle_breadcrumb_rooms(const event &, vm::eval &);
	extern hookfn<vm::eval &> hook_breadcrumb_rooms;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix Breadcrumb Rooms"
};

decltype(ircd::m::hook_breadcrumb_rooms)
ircd::m::hook_breadcrumb_rooms
{
	handle_breadcrumb_rooms,
	{
		{ "_site",      "vm.effect"                        },
		{ "type",       "ircd.account_data"                },
		{ "state_key",  "im.vector.riot.breadcrumb_rooms"  },
	},
};

void
ircd::m::handle_breadcrumb_rooms(const event &event,
                                 vm::eval &eval)
try
{
	const m::user::id &sender
	{
		at<"sender"_>(event)
	};

	const m::user::room user_room
	{
		sender
	};

	// We only want to hook events in the user's user room.
	if(at<"room_id"_>(event) != user_room.room_id)
		return;

	const auto &content
	{
		json::get<"content"_>(event)
	};

	const json::array &rooms
	{
		content.get("rooms")
	};

	handle_breadcrumb_rooms_focus_out(event, eval, rooms);
	handle_breadcrumb_rooms_focus_in(event, eval, rooms);
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "breadcrumb_rooms hook :%s",
		e.what(),
	};
}

void
ircd::m::handle_breadcrumb_rooms_focus_in(const event &event,
                                          vm::eval &eval,
                                          const json::array &rooms)
{
	const room::id &focus_in
	{
		valid(id::ROOM, json::string{rooms[0]})?
			room::id{json::string{rooms[0]}}:
			room::id{}
	};

	if(!focus_in)
		return;

	const size_t prefetched
	{
		m::room::events::prefetch_viewport(focus_in)
	};

	log::debug
	{
		m::log, "Prefetched %zu recent events to focus %s for %s",
		prefetched,
		string_view{focus_in},
		at<"sender"_>(event),
	};
}

void
ircd::m::handle_breadcrumb_rooms_focus_out(const event &event,
                                           vm::eval &eval,
                                           const json::array &rooms)
{
	const room::id &focus_out
	{
		valid(id::ROOM, json::string{rooms[1]})?
			room::id{json::string{rooms[1]}}:
			room::id{}
	};

	if(!focus_out)
		return;

	log::debug
	{
		m::log, "%s for %s out of focus",
		string_view{focus_out},
		at<"sender"_>(event),
	};
}

ircd::m::event::id::buf
IRCD_MODULE_EXPORT
ircd::m::breadcrumb_rooms::set(const json::array &rooms)
const
{
	const json::strung object
	{
		json::members
		{
			{ "rooms", rooms }
		}
	};

	return account_data.set("im.vector.riot.breadcrumb_rooms", object);
}

bool
IRCD_MODULE_EXPORT
ircd::m::breadcrumb_rooms::for_each(const closure_bool &closure)
const
{
	bool ret{true};
	get(std::nothrow, [&closure, &ret]
	(const json::array &rooms)
	{
		for(const json::string &room : rooms)
			if(!closure(room))
			{
				ret = false;
				break;
			}
	});

	return ret;
}

void
IRCD_MODULE_EXPORT
ircd::m::breadcrumb_rooms::get(const closure &closure)
const
{
	if(!get(std::nothrow, closure))
		throw m::NOT_FOUND
		{
			"User has no breadcrumb_rooms set in their account_data."
		};
}

bool
IRCD_MODULE_EXPORT
ircd::m::breadcrumb_rooms::get(std::nothrow_t,
                               const closure &closure)
const
{
	return account_data.get(std::nothrow, "im.vector.riot.breadcrumb_rooms", [&closure]
	(const string_view &key, const json::object &object)
	{
		const json::array &rooms
		{
			object["rooms"]
		};

		closure(rooms);
	});
}
