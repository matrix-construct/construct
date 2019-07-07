// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::mapi::header
IRCD_MODULE
{
	"Client Sync :Room Summary"
};

namespace ircd::m::sync
{
	static bool room_summary_append_counts(data &);
	static bool room_summary_append_heroes(data &);

	static bool room_summary_polylog(data &);
	static bool room_summary_linear(data &);
	extern item room_summary;
}

decltype(ircd::m::sync::room_summary)
ircd::m::sync::room_summary
{
	"rooms.summary",
	room_summary_polylog,
	room_summary_linear,
	{
		{ "initial",  true },
	}
};

bool
ircd::m::sync::room_summary_linear(data &data)
{
	if(!data.event_idx)
		return false;

	if(!data.membership)
		return false;

	if(!data.room)
		return false;

	assert(data.event);
	if(at<"type"_>(*data.event) != "m.room.member")
		return false;

	const auto &room{*data.room};
	const m::room::members members
	{
		room
	};

	json::stack::object rooms
	{
		*data.out, "rooms"
	};

	json::stack::object membership_
	{
		*data.out, data.membership
	};

	json::stack::object room_
	{
		*data.out, data.room->room_id
	};

	json::stack::object summary
	{
		*data.out, "summary"
	};

	bool ret{false};
	ret |= room_summary_append_counts(data);
	ret |= room_summary_append_heroes(data);
	return ret;
}

bool
ircd::m::sync::room_summary_polylog(data &data)
{
	bool ret{false};
	ret |= room_summary_append_counts(data);
	ret |= room_summary_append_heroes(data);
	return ret;
}

bool
ircd::m::sync::room_summary_append_heroes(data &data)
{
	json::stack::array m_heroes
	{
		*data.out, "m.heroes"
	};

	static const auto count{6}, bufsz{48}, limit{12};
	std::array<char[bufsz], count> buf;
	std::array<string_view, count> last;
	size_t ret(0), i(0);
	m::room::messages it
	{
		*data.room
	};

	const auto already{[&last, &ret]
	(const string_view &sender) -> bool
	{
		return std::any_of(begin(last), begin(last)+ret, [&sender]
		(const auto &last)
		{
			return startswith(last, sender);
		});
	}};

	for(; it && ret < count && i < limit; --it, ++i)
	{
		const auto &event_idx(it.event_idx());
		m::get(std::nothrow, event_idx, "sender", [&]
		(const auto &sender)
		{
			if(already(sender))
				return;

			m_heroes.append(sender);
			last.at(ret) = strlcpy(buf.at(ret), sender);
			++ret;
		});
	};

	return ret;
}

bool
ircd::m::sync::room_summary_append_counts(data &data)
{
	const m::room::members members
	{
		*data.room
	};

	const auto joined_members_count
	{
		members.count("join")
	};

	json::stack::member
	{
		*data.out, "m.joined_member_count", json::value
		{
			long(joined_members_count)
		}
	};

	// TODO: XXX
	// Invited member count is omitted for now. We don't yet enjoy an
	// optimized query for the invited member count like we do with the
	// joined member count.
	const long invited_members_count
	{
		0UL
	};

//	json::stack::member
//	{
//		*data.out, "m.invited_member_count", json::value
//		{
//			long(members.count("invite"))
//		}
//	};

	return joined_members_count || invited_members_count;
}
