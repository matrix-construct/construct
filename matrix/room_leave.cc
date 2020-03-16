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
ircd::m::leave(const room &room,
               const id::user &user_id)
{
	if(!exists(room))
		return {};

	json::iov event;
	json::iov content;
	const json::iov::push push[]
	{
		{ event,    { "type",        "m.room.member"  }},
		{ event,    { "sender",      user_id          }},
		{ event,    { "state_key",   user_id          }},
		{ content,  { "membership",  "leave"          }},
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
