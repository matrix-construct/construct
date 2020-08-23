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
	static void handle_breadcrumbs_focus_out(const event &, vm::eval &, const json::array &);
	static void handle_breadcrumbs_focus_in(const event &, vm::eval &, const json::array &);
	static void handle_breadcrumbs(const event &, vm::eval &);
	extern hookfn<vm::eval &> hook_breadcrumbs;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix Breadcrumb Rooms"
};

decltype(ircd::m::hook_breadcrumbs)
ircd::m::hook_breadcrumbs
{
	handle_breadcrumbs,
	{
		{ "_site",      "vm.effect"                       },
		{ "type",       "ircd.account_data"               },
		{ "state_key",  "im.vector.setting.breadcrumbs"   },
	},
};

void
ircd::m::handle_breadcrumbs(const event &event,
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
		content.get("recent_rooms")
	};

	handle_breadcrumbs_focus_out(event, eval, rooms);
	handle_breadcrumbs_focus_in(event, eval, rooms);
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "breadcrumbs hook :%s",
		e.what(),
	};
}

void
ircd::m::handle_breadcrumbs_focus_in(const event &event,
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
ircd::m::handle_breadcrumbs_focus_out(const event &event,
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
