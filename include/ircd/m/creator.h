// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_CREATOR_H

namespace ircd::m
{
	id::user creator(const event &);
	id::user creation(const event &);

	bool creator(const event &, const id::user &);
	bool creation(const event &, const id::user &);
}

/// Events that are not type=m.room.create will return false; the sender field
/// will be tried if available, otherwise content.creator will be tried.
inline bool
ircd::m::creation(const event &event,
                  const id::user &user)
{
	if(json::get<"type"_>(event) != "m.room.create")
		return false;

	return creator(event, user);
}

/// The sender field will be tried if available, otherwise content.creator will
/// be tried.
inline bool
ircd::m::creator(const event &event,
                 const id::user &user)
{
	assert(defined(user));
	if(likely(defined(json::get<"sender"_>(event))))
		if(json::get<"sender"_>(event) == user)
			return true;

	const json::string creator
	{
		json::get<"content"_>(event).get("creator")
	};

	return creator == user;
}

/// Events that are not type=m.room.create will return empty; the sender
/// will be tried if available, otherwise content.creator will be tried.
inline ircd::m::id::user
ircd::m::creation(const event &event)
{
	if(json::get<"type"_>(event) != "m.room.create")
		return id::user{};

	return creator(event);
}

/// The sender will be tried if available, otherwise content.creator will be
/// tried.
inline ircd::m::id::user
ircd::m::creator(const event &event)
{
	if(likely(defined(json::get<"sender"_>(event))))
		return json::get<"sender"_>(event);

	const json::string creator
	{
		json::get<"content"_>(event).get("creator")
	};

	return creator;
}
