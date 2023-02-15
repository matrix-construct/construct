// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::bridge
{
	event::id::buf set(const string_view &file);
	event::id::buf add(const string_view &file);
	event::id::buf del(const string_view &file);
}

ircd::mapi::header
IRCD_MODULE
{
	"Bridges (Application Services) :Registration"
};

ircd::m::event::id::buf
IRCD_MODULE_EXPORT_CODE
ircd::m::bridge::del(const string_view &id)
{
	if(!m::bridge::config::exists(id))
		throw error
		{
			"Configuration for '%s' doesn't exist.",
			string_view{id},
		};

	const m::room::id::buf room_id
	{
		id, origin(my())
	};

	if(!m::exists(room_id))
		throw error
		{
			"Bridge room %s is missing.",
			string_view{room_id},
		};

	const m::user::id::buf user_id
	{
		id, origin(my())
	};

	if(!m::exists(user_id))
		throw error
		{
			"Bridge user %s is missing.",
			string_view{user_id},
		};

	const m::room room
	{
		room_id
	};

	const auto event_idx
	{
		room.get("ircd.bridge", id)
	};

	const auto event_id
	{
		m::event_id(event_idx)
	};

	const auto redact_id
	{
		m::redact(room, user_id, event_id, "deleted")
	};

	return redact_id;
}

ircd::m::event::id::buf
IRCD_MODULE_EXPORT_CODE
ircd::m::bridge::add(const string_view &file)
{
	const fs::fd fd{file};
	const fs::map map{fd};
	const m::bridge::config config
	{
		json::object{map}
	};

	if(m::bridge::config::exists(at<"id"_>(config)))
		throw error
		{
			"Configuration for '%s' already exists.",
			string_view{at<"id"_>(config)},
		};

	const m::user::id::buf user_id
	{
		at<"sender_localpart"_>(config), origin(my())
	};

	if(m::exists(user_id))
		throw error
		{
			"Bridge user %s already exists.",
			string_view{user_id},
		};

	const m::room::id::buf room_id
	{
		at<"sender_localpart"_>(config), origin(my())
	};

	if(m::exists(room_id))
		throw error
		{
			"Bridge room %s already exists.",
			string_view{room_id},
		};

	return set(file);
}

ircd::m::event::id::buf
IRCD_MODULE_EXPORT_CODE
ircd::m::bridge::set(const string_view &file)
{
	const fs::fd fd
	{
		file
	};

	std::string content
	{
		fs::read(fd)
	};

	m::bridge::config config
	{
		json::object(content)
	};

	const auto update
	{
		[&content, &config]
		{
			content = json::strung(config);
			config = json::object(content);
		}
	};

	const m::user::id::buf user_id
	{
		at<"sender_localpart"_>(config), origin(my())
	};

	const m::room::id::buf room_id
	{
		at<"sender_localpart"_>(config), origin(my())
	};

	const bool user_exists
	{
		m::exists(user_id)
	};

	const bool room_exists
	{
		m::exists(room_id)
	};

	const bool has_as_token
	{
		!empty(json::get<"as_token"_>(config))
	};

	const bool has_as_token_prefix
	{
		startswith(json::get<"as_token"_>(config), "bridge_")
	};

	if(json::get<"id"_>(config) != user_id.localname())
		throw error
		{
			"sender_localpart '%s' must match id '%s'",
			json::get<"sender_localpart"_>(config),
			json::get<"id"_>(config),
		};

	if(!user_exists)
		m::create(user_id);

	if(!room_exists)
		m::create(room_id, user_id);

	if(!is_oper(user_id))
		m::user(user_id).oper();

	const m::user::tokens tokens
	{
		user_id
	};

	if(!has_as_token)
	{
		char buf[2][128];
		strlcpy(buf[0], "bridge_");
		strlcat(buf[0], m::user::tokens::generate(buf[1]));
		const string_view token{buf[0]};
		tokens.add(token);
		json::get<"as_token"_>(config) = token;
		update();
	}
	else if(!has_as_token_prefix)
	{
		char buf[128];
		strlcpy(buf, "bridge_");
		strlcat(buf, json::get<"as_token"_>(config));
		const string_view token{buf};
		tokens.add(token);
		json::get<"as_token"_>(config) = token;
		update();
	}
	else if(!tokens.check(json::get<"as_token"_>(config)))
		tokens.add(json::get<"as_token"_>(config));

	// Check if the config is the same as the existing one so we don't create
	// identical state; update() ensures canonical serialization first.
	update();
	event::idx exist_idx{0};
	m::bridge::config::get(std::nothrow, at<"id"_>(config), [&exist_idx, &config]
	(const auto &event_idx, const auto &event, const auto &exist)
	{
		if(exist.source == config.source)
			exist_idx = event_idx;
	});

	const auto event_id
	{
		!exist_idx?
			m::send(room_id, user_id, "ircd.bridge", at<"id"_>(config), content):
			m::event_id(exist_idx)
	};

	return event_id;
}
