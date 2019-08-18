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
	static void room_message_auth(const event &, room::auth::hookdata &);
	extern hookfn<room::auth::hookdata &> room_message_auth_hook;

	static void room_message_notify(const event &, vm::eval &);
	extern hookfn<vm::eval &> room_message_notify_hook;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix m.room.message"
};

decltype(ircd::m::room_message_notify_hook)
ircd::m::room_message_notify_hook
{
	room_message_notify,
	{
		{ "_site",  "vm.notify"       },
		{ "type",   "m.room.message"  },
	}
};

void
ircd::m::room_message_notify(const event &event,
                             vm::eval &eval)
{
	const auto &content
	{
		json::get<"content"_>(event)
	};

	const json::string &body
	{
		content.get("body")
	};

	const json::string &msgtype
	{
		content.get("msgtype")
	};

	log::info
	{
		m::log, "%s said %s in %s %s :%s%s",
		json::get<"sender"_>(event),
		string_view{event.event_id},
		json::get<"room_id"_>(event),
		msgtype,
		trunc(body, 128),
		size(body) > 128? "..."_sv : string_view{}
	};
}

decltype(ircd::m::room_message_auth_hook)
ircd::m::room_message_auth_hook
{
	room_message_auth,
	{
		{ "_site",  "room.auth"       },
		{ "type",   "m.room.message"  },
	}
};

void
ircd::m::room_message_auth(const event &event,
                           room::auth::hookdata &data)
{
	using FAIL = m::room::auth::FAIL;
	using conforms = m::event::conforms;
	assert(json::get<"type"_>(event) == "m.room.message");

	const auto &content
	{
		json::get<"content"_>(event)
	};

	if(!user::highlight::match_at_room)
		return;

	const json::string &body
	{
		content.get("body")
	};

	if(likely(!startswith(body, "@room")))
		return;

	const room::power power
	{
		data.auth_power?
			*data.auth_power : m::event{},
		*data.auth_create
	};

	const auto &user_level
	{
		power.level_user(at<"sender"_>(event))
	};

	int64_t required_level
	{
		room::power::default_power_level
	};

	power.for_each("notifications", room::power::closure_bool{[&required_level]
	(const auto &name, const auto &level)
	{
		if(name != "room")
			return true;

		required_level = level;
		return false;
	}});

	if(user_level < required_level)
		throw FAIL
		{
			"Insufficient power level to highlight the room (have:%ld require:%ld).",
			user_level,
			required_level
		};
}
