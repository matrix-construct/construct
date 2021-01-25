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

ircd::m::user::reading::reading(const user &user)
{
	if(!(room_id = viewing(user)))
		return;

	const user::room user_room
	{
		user
	};

	const auto last_event_idx
	{
		user_room.get(std::nothrow, "ircd.read", room_id)
	};

	const bool last_content_prefetched
	{
		prefetch(last_event_idx, "content")
	};

	time_t last_ots {0};
	get<time_t>(last_event_idx, "origin_server_ts", last_ots);
	this->last_ots = milliseconds(last_ots) / 1000;

	get(std::nothrow, last_event_idx, "content", [this]
	(const json::object &content)
	{
		this->last_ts = content.get<milliseconds>("ts");
		this->last = json::string
		{
			content["event_id"]
		};
	});

	const user::room_account_data room_account_data
	{
		user, room_id
	};

	room_account_data.get(std::nothrow, "m.fully_read", [this]
	(const string_view &key, const json::object &content)
	{
		this->full = json::string
		{
			content["event_id"]
		};
	});

	//TODO: XXX
	// full_ots

	presence::get(std::nothrow, user, [this]
	(const json::object &event)
	{
		this->currently_active = event.get<bool>("currently_active", false);
	});
}

ircd::m::room::id::buf
ircd::m::viewing(const user &user,
                 size_t i)
{
	const m::breadcrumbs breadcrumbs
	{
		user
	};

	room::id::buf ret;
	breadcrumbs.for_each([&ret, &i]
	(const auto &room_id)
	{
		if(i-- > 0)
			return true;

		ret = room_id;
		return false;
	});

	return ret;
}

bool
ircd::m::is_oper(const user &user)
{
	const m::room::id::buf control_room_id
	{
		"!control", my_host()
	};

	return m::membership(control_room_id, user, "join");
}

bool
ircd::m::active(const user &user)
{
	const m::user::room user_room
	{
		user.user_id
	};

	const m::event::idx &event_idx
	{
		user_room.get(std::nothrow, "ircd.account", "active")
	};

	return m::query(std::nothrow, event_idx, "content", []
	(const json::object &content)
	{
		return content.get<bool>("value", false);
	});
}

bool
ircd::m::exists(const user &user)
{
	return exists(user.user_id);
}

bool
ircd::m::exists(const user::id &user)
{
	// The way we know a user exists is testing if their room exists.
	const user::room user_room
	{
		user
	};

	return m::exists(user_room);
}

bool
ircd::m::my(const user &user)
{
	return my(user.user_id);
}

ircd::m::user
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
	return create(room_id, me(), "user");
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

/// Generates a user-room ID into buffer; see room_id() overload.
ircd::m::id::room::buf
ircd::m::user::room_id()
const
{
	ircd::m::id::room::buf buf;
	return buf.assigned(room_id(buf));
}

/// This generates a room mxid for the "user's room" essentially serving as
/// a database mechanism for this specific user. This room_id is a hash of
/// the user's full mxid.
///
ircd::m::id::room
ircd::m::user::room_id(const mutable_buffer &buf)
const
{
	assert(!empty(user_id));
	const ripemd160::buf hash
	{
		ripemd160{user_id}
	};

	char b58[size(hash) * 2];
	return
	{
		buf, b58::encode(b58, hash), origin(my())
	};
}

ircd::m::event::id::buf
ircd::m::user::activate()
{
	const m::user::room user_room
	{
		user_id
	};

	return send(user_room, m::me(), "ircd.account", "active", json::members
	{
		{ "value", true }
	});
}

ircd::m::event::id::buf
ircd::m::user::deactivate()
{
	const m::user::room user_room
	{
		user_id
	};

	return send(user_room, m::me(), "ircd.account", "active", json::members
	{
		{ "value", false }
	});
}

ircd::m::event::id::buf
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
ircd::m::user::is_password(const string_view &password)
const try
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

	return b64::encode_unpadded(out, hash);
}

//
// user::room
//

ircd::m::user::room::room(const m::user::id &user_id,
                          const vm::copts *const &copts,
                          const event::fetch::opts *const &fopts)
:room
{
	m::user{user_id}, copts, fopts
}
{
}

ircd::m::user::room::room(const m::user &user,
                          const vm::copts *const &copts,
                          const event::fetch::opts *const &fopts)
:user{user}
,room_id{user.room_id()}
{
	static_cast<m::room &>(*this) =
	{
		room_id, copts, fopts
	};
}

bool
ircd::m::user::room::is(const room::id &room_id,
                        const user::id &user_id)
{
	const user::room user_room{user_id};
	return user_room.room_id == room_id;
}
