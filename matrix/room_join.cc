// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::m::event::id::buf
ircd::m::join(const room::alias &room_alias,
              const user::id &user_id)
{
	if(unlikely(!my(user_id)))
		throw panic
		{
			"Can only join my users."
		};

	const room::id::buf room_id
	{
		m::room_id(room_alias)
	};

	if(room::bootstrap::required(room_id))
	{
		const auto &remote
		{
			room_alias.host()
		};

		m::event::id::buf ret;
		m::room::bootstrap
		{
			ret,
			room_id,
			user_id,
			{ &remote, 1 },
		};

		return ret;
	}

	const m::room room
	{
		room_id
	};

	return m::join(room, user_id);
}

ircd::m::event::id::buf
ircd::m::join(const m::room &room,
              const m::id::user &user_id,
              const vector_view<const string_view> &remotes)
{
	if(unlikely(!my(user_id)))
		throw panic
		{
			"Can only join my users."
		};

	// Branch for when nothing is known about the room.
	if(room::bootstrap::required(room))
	{
		// The bootstrap condcts a blocking make_join and issues a join
		// event, returning the event_id; afterward asynchronously it will
		// attempt a send_join, and then process the room events.
		m::event::id::buf ret;
		m::room::bootstrap
		{
			ret,
			room.room_id,
			user_id,
			remotes,
		};

		return ret;
	}

	if(membership(room, user_id, "join"))
	{
		const auto &event_idx
		{
			room.get(std::nothrow, "m.room.member", user_id)
		};

		const event::id::buf event_id
		{
			event_idx?
				m::event_id(std::nothrow, event_idx):
				event::id::buf{}
		};

		//TODO: check duplicate content
		//if(event_id)
		//	return event_id;
	}

	json::iov event;
	json::iov content;
	const json::iov::push push[]
	{
		{ event,    { "type",        "m.room.member"  }},
		{ event,    { "sender",      user_id          }},
		{ event,    { "state_key",   user_id          }},
		{ content,  { "membership",  "join"           }},
	};

	const m::user user{user_id};
	const m::user::profile profile{user};

	char displayname_buf[256];
	const string_view displayname
	{
		profile.get(displayname_buf, "displayname")
	};

	char avatar_url_buf[256];
	const string_view avatar_url
	{
		profile.get(avatar_url_buf, "avatar_url")
	};

	const json::iov::add _displayname
	{
		content, !empty(displayname),
		{
			"displayname", [&displayname]() -> json::value
			{
				return displayname;
			}
		}
	};

	const json::iov::add _avatar_url
	{
		content, !empty(avatar_url),
		{
			"avatar_url", [&avatar_url]() -> json::value
			{
				return avatar_url;
			}
		}
	};

	return commit(room, event, content);
}
