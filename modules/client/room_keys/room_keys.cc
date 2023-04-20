// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "room_keys.h"

ircd::mapi::header
IRCD_MODULE
{
	"Client :e2e Room Keys"
};

std::tuple<ircd::string_view, ircd::string_view, ircd::string_view>
ircd::m::unmake_state_key(const string_view &state_key)
{
	assert(state_key);
	string_view part[3];
	const auto parts
	{
		tokens(state_key, ":::", part)
	};

	assert(parts == 3);
	if(unlikely(!m::valid(id::ROOM, part[0])))
		part[0] = {};

	if(unlikely(!lex_castable<ulong>(part[2])))
		part[2] = {};

	return std::make_tuple
	(
		part[0], part[1], part[2]
	);
}

ircd::string_view
ircd::m::make_state_key(const mutable_buffer &buf,
                        const string_view &room_id,
                        const string_view &session_id,
                        const event::idx &version)
{
	assert(room_id);
	assert(m::valid(id::ROOM, room_id));
	assert(session_id);
	assert(session_id != "sessions");
	assert(version != 0);
	return fmt::sprintf
	{
		buf, "%s:::%s:::%u",
		room_id,
		session_id,
		version,
	};
}
