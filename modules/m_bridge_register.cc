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
}

ircd::mapi::header
IRCD_MODULE
{
	"Bridges (Application Services) :Registration"
};

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

	const auto event_id
	{
		m::send(room_id, user_id, "ircd.bridge", at<"id"_>(config), content)
	};

	return event_id;
}
