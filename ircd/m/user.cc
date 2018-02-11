// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/m/m.h>

const ircd::m::room::id::buf
accounts_room_id
{
	"accounts", ircd::my_host()
};

ircd::m::room
ircd::m::user::accounts
{
	accounts_room_id
};

const ircd::m::room::id::buf
sessions_room_id
{
	"sessions", ircd::my_host()
};

ircd::m::room
ircd::m::user::sessions
{
	sessions_room_id
};

/// Register the user by joining them to the accounts room.
///
/// The content of the join event may store keys including the registration
/// options. Once this call completes the join was successful and the user is
/// registered, otherwise throws.
void
ircd::m::user::activate(const json::members &contents)
try
{
	json::iov content;
	json::iov::push push[]
	{
		{ content, { "membership", "join" }},
	};

	size_t i(0);
	json::iov::push _content[contents.size()];
	for(const auto &member : contents)
		new (_content + i++) json::iov::push(content, member);

	send(accounts, me.user_id, "ircd.user", user_id, content);
	join(control, user_id);
}
catch(const m::ALREADY_MEMBER &e)
{
	throw m::error
	{
		http::CONFLICT, "M_USER_IN_USE", "The desired user ID is already in use."
	};
}

void
ircd::m::user::deactivate(const json::members &contents)
{
	json::iov content;
	json::iov::push push[]
	{
		{ content, { "membership", "leave" }},
	};

	size_t i(0);
	json::iov::push _content[contents.size()];
	for(const auto &member : contents)
		new (_content + i++) json::iov::push(content, member);

	send(accounts, me.user_id, "ircd.user", user_id, content);
}

void
ircd::m::user::password(const string_view &password)
try
{
	//TODO: ADD SALT
	char b64[64], hash[32];
	sha256{hash, password};
	const auto digest{b64encode_unpadded(b64, hash)};
	send(accounts, me.user_id, "ircd.password", user_id,
	{
		{ "sha256", digest }
	});
}
catch(const m::ALREADY_MEMBER &e)
{
	throw m::error
	{
		http::CONFLICT, "M_USER_IN_USE", "The desired user ID is already in use."
	};
}

bool
ircd::m::user::is_password(const string_view &supplied_password)
const
{
	//TODO: ADD SALT
	char b64[64], hash[32];
	sha256{hash, supplied_password};
	const auto supplied_hash
	{
		b64encode_unpadded(b64, hash)
	};

	static const string_view type{"ircd.password"};
	const string_view &state_key{user_id};
	const room room
	{
		accounts.room_id
	};

	bool ret{false};
	room::state{room}.get(type, state_key, [&supplied_hash, &ret]
	(const m::event &event)
	{
		const json::object &content
		{
			json::at<"content"_>(event)
		};

		const auto &correct_hash
		{
			unquote(content.at("sha256"))
		};

		ret = supplied_hash == correct_hash;
	});

	return ret;
}

bool
ircd::m::user::is_active()
const
{
	bool ret{false};
	room::state{accounts}.get("ircd.user", user_id, [&ret]
	(const m::event &event)
	{
		const json::object &content
		{
			at<"content"_>(event)
		};

		ret = unquote(content.at("membership")) == "join";
	});

	return ret;
}
