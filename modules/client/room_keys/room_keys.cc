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

std::tuple<int64_t, int64_t>
ircd::m::count_etag(const room::state &state,
                    const event::idx &version)
{
	char version_buf[64];
	const auto version_str
	{
		lex_cast(version, version_buf)
	};

	uint64_t count(0), etag(0);
	state.for_each("ircd.room_keys.key", [&]
	(const string_view &type, const string_view &state_key, const event::idx &event_idx)
	{
		const auto &[room_id, session_id, _version_str]
		{
			unmake_state_key(state_key)
		};

		if(_version_str != version_str)
			return true;

		etag += event_idx;
		count += 1;
		return true;
	});

	return
	{
		int64_t(count),
		int64_t(etag),
	};
}

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
