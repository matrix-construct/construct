// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::string_view
ircd::m::display_name(const mutable_buffer &out,
                      const room &room)
{
	const room::state state
	{
		room
	};

	// 1. If the room has an m.room.name state event with a non-empty name
	// field, use the name given by that field.
	string_view ret;
	{
		const event::idx event_idx
		{
			state.get(std::nothrow, "m.room.name", "")
		};

		m::get(std::nothrow, event_idx, "content", [&out, &ret]
		(const json::object &content)
		{
			const json::string &name
			{
				content.get("name")
			};

			ret = strlcpy
			{
				out, name
			};
		});
	}

	// 2. If the room has an m.room.canonical_alias state event with a
	// non-empty alias field, use the alias given by that field as the name.
	if(!ret)
	{
		const event::idx event_idx
		{
			state.get(std::nothrow, "m.room.canonical_alias", "")
		};

		m::get(std::nothrow, event_idx, "content", [&out, &ret]
		(const json::object &content)
		{
			const json::string &alias
			{
				content.get("alias")
			};

			ret = strlcpy
			{
				out, alias
			};
		});
	}

	// 3. If neither of the above conditions are met, the client can optionally
	// guess an alias from the m.room.alias events in the room. This is a
	// temporary measure while clients do not promote canonical aliases as
	// prominently. This step may be removed in a future version of the
	// specification.

	// 4. If none of the above conditions are met, a name should be composed
	// based on the members of the room. Clients should consider m.room.member
	// events for users other than the logged-in user, as defined below.

	// i. If the number of m.heroes for the room are greater or equal to
	// m.joined_member_count + m.invited_member_count - 1, then use the
	// membership events for the heroes to calculate display names for the
	// users (disambiguating them if required) and concatenating them. For
	// example, the client may choose to show "Alice, Bob, and Charlie
	// (@charlie:example.org)" as the room name. The client may optionally
	// limit the number of users it uses to generate a room name.

	// ii. If there are fewer heroes than m.joined_member_count +
	// m.invited_member_count - 1, and m.joined_member_count +
	// m.invited_member_count is greater than 1, the client should use the
	// heroes to calculate display names for the users (disambiguating them if
	// required) and concatenating them alongside a count of the remaining
	// users. For example, "Alice, Bob, and 1234 others".

	// iii. If m.joined_member_count + m.invited_member_count is less than or
	// equal to 1 (indicating the member is alone), the client should use the
	// rules above to indicate that the room was empty. For example, "Empty Room
	// (was Alice)", "Empty Room (was Alice and 1234 others)", or "Empty Room"
	// if there are no heroes.

	return ret;
}
