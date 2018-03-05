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

/// ID of the room which indexes all users (an instance of the room is
/// provided below).
const ircd::m::room::id::buf
users_room_id
{
	"users", ircd::my_host()
};

/// The users room is the database of all users. It primarily serves as an
/// indexing mechanism and for top-level user related keys. Accounts
/// registered on this server will be among state events in this room.
/// Users do not have access to this room, it is used internally.
///
ircd::m::room
ircd::m::user::users
{
	users_room_id
};

/// ID of the room which stores ephemeral tokens (an instance of the room is
/// provided below).
const ircd::m::room::id::buf
tokens_room_id
{
	"tokens", ircd::my_host()
};

/// The tokens room serves as a key-value lookup for various tokens to
/// users, etc. It primarily serves to store access tokens for users. This
/// is a separate room from the users room because in the future it may
/// have an optimized configuration as well as being more easily cleared.
///
ircd::m::room
ircd::m::user::tokens
{
	tokens_room_id
};

bool
ircd::m::exists(const user::id &user_id)
{
	return user::users.has("ircd.user", user_id);
}

bool
ircd::m::my(const user &user)
{
	return my(user.user_id);
}

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
	const sha256::buf hash
	{
		sha256{user_id}
	};

	char b58[size(hash) * 2];
	return
	{
		buf, b58encode(b58, hash), my_host()
	};
}

ircd::string_view
ircd::m::user::gen_access_token(const mutable_buffer &buf)
{
	static const size_t token_max{32};
	static const auto &token_dict{rand::dict::alpha};

	const mutable_buffer out
	{
		data(buf), std::min(token_max, size(buf))
	};

	return rand::string(token_dict, out);
}

//
// user::room
//

ircd::m::user::room::room(const m::user::id &user_id)
:room{m::user{user_id}}
{}

ircd::m::user::room::room(const m::user &user)
:user{user}
,room_id{user.room_id()}
{
	static_cast<m::room &>(*this) = room_id;
}
