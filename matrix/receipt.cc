// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::receipt::log)
ircd::m::receipt::log
{
	"m.receipt"
};

ircd::m::event::id::buf
ircd::m::receipt::read(const m::room::id &room_id,
                       const m::user::id &user_id,
                       const m::event::id &event_id,
                       const json::object &options)
{
	const m::user::room user_room
	{
		user_id
	};

	const auto evid
	{
		send(user_room, user_id, "ircd.read", room_id,
		{
			{ "event_id",    event_id                                       },
			{ "ts",          options.get("ts", ircd::time<milliseconds>())  },
			{ "m.hidden",    options.get("m.hidden", false)                 },
		})
	};

	log::info
	{
		log, "%s read by %s in %s options:%s",
		string_view{event_id},
		string_view{user_id},
		string_view{room_id},
		string_view{options},
	};

	return evid;
}

bool
ircd::m::receipt::get(const m::room::id &room_id,
                      const m::user::id &user_id,
                      const m::event::id::closure &closure)
{
	const m::user::room user_room
	{
		user_id
	};

	const auto event_idx
	{
		user_room.get(std::nothrow, "ircd.read", room_id)
	};

	return m::get(std::nothrow, event_idx, "content", [&closure]
	(const json::object &content)
	{
		const json::string &event_id
		{
			content["event_id"]
		};

		closure(event_id);
	});
}


/// Does the user wish to not send receipts for events sent by its specific
/// sender?
bool
ircd::m::receipt::ignoring(const m::user &user,
                           const m::event::id &event_id)
{
	bool ret{false};
	m::get(std::nothrow, event_id, "sender", [&ret, &user]
	(const string_view &sender)
	{
		const m::user::room user_room{user};
		ret = user_room.has("ircd.read.ignore", sender);
	});

	return ret;
}

/// Does the user wish to not send receipts for events for this entire room?
bool
ircd::m::receipt::ignoring(const m::user &user,
                           const m::room::id &room_id)
{
	const m::user::room user_room{user};
	return user_room.has("ircd.read.ignore", room_id);
}

bool
ircd::m::receipt::freshest(const m::room::id &room_id,
                           const m::user::id &user_id,
                           const m::event::id &event_id)
try
{
	const m::user::room user_room
	{
		user_id
	};

	const m::event::idx event_idx
	{
		index(std::nothrow, event_id)
	};

	if(!event_idx)
		return true;

	const auto last_idx
	{
		user_room.get(std::nothrow, "ircd.read", room_id)
	};

	if(!last_idx)
		return true;

	if(last_idx < event_idx)
		return true;

	return false;
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "Freshness of receipt in %s from %s for %s :%s",
		string_view{room_id},
		string_view{user_id},
		string_view{event_id},
		e.what()
	};

	return true;
}

bool
ircd::m::receipt::exists(const m::room::id &room_id,
                         const m::user::id &user_id,
                         const m::event::id &event_id)
{
	const m::user::room user_room
	{
		user_id
	};

	bool ret{false};
	user_room.get(std::nothrow, "ircd.read", room_id, [&ret, &event_id]
	(const m::event &event)
	{
		const auto &content
		{
			at<"content"_>(event)
		};

		ret = unquote(content.get("event_id")) == event_id;
	});

	return ret;
}
