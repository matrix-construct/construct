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
	static void user_highlight_auth(const event &, room::auth::hookdata &);
	extern hookfn<room::auth::hookdata &> user_highlight_auth_hook;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix @room highlight authentication"
};

decltype(ircd::m::user_highlight_auth_hook)
ircd::m::user_highlight_auth_hook
{
	user_highlight_auth,
	{
		{ "_site",  "room.auth"       },
		{ "type",   "m.room.message"  },
	}
};

void
ircd::m::user_highlight_auth(const event &event,
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
