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
	static string_view gen_password_hash(const mutable_buffer &, const string_view &);
	static room create_user_room(const user::id &, const room::id &, const json::members &contents);
}

ircd::m::user
IRCD_MODULE_EXPORT
ircd::m::create(const m::user::id &user_id,
                const json::members &contents)
{
	const m::user user
	{
		user_id
	};

	const m::room::id::buf room_id
	{
		user.room_id()
	};

	const m::room room
	{
		create_user_room(user_id, room_id, contents)
	};

	return user;
}

ircd::m::room
ircd::m::create_user_room(const user::id &user_id,
                          const room::id &room_id,
                          const json::members &contents)
try
{
	return create(room_id, m::me.user_id, "user");
}
catch(const std::exception &e)
{
	if(m::exists(room_id))
		return room_id;

	log::error
	{
		log, "Failed to create user %s room %s :%s",
		string_view{user_id},
		string_view{room_id},
		e.what()
	};

	throw;
}

//
// user::user
//

ircd::m::event::id::buf
IRCD_MODULE_EXPORT
ircd::m::user::password(const string_view &password)
{
	char buf[64];
	const auto supplied
	{
		gen_password_hash(buf, password)
	};

	const m::user::room user_room
	{
		user_id
	};

	return send(user_room, user_id, "ircd.password", user_id,
	{
		{ "sha256", supplied }
	});
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::is_password(const string_view &password)
const noexcept try
{
	char buf[64];
	const auto supplied
	{
		gen_password_hash(buf, password)
	};

	bool ret{false};
	const m::user::room user_room
	{
		user_id
	};

	const ctx::uninterruptible::nothrow ui;
	user_room.get("ircd.password", user_id, [&supplied, &ret]
	(const m::event &event)
	{
		const json::object &content
		{
			json::at<"content"_>(event)
		};

		const auto &correct
		{
			unquote(content.at("sha256"))
		};

		ret = supplied == correct;
	});

	return ret;
}
catch(const m::NOT_FOUND &e)
{
	return false;
}
catch(const std::exception &e)
{
	log::critical
	{
		"is_password__user(): %s %s",
		string_view{user_id},
		e.what()
	};

	return false;
}

ircd::string_view
ircd::m::gen_password_hash(const mutable_buffer &out,
                           const string_view &supplied_password)
{
	//TODO: ADD SALT
	const sha256::buf hash
	{
		sha256{supplied_password}
	};

	return b64encode_unpadded(out, hash);
}
