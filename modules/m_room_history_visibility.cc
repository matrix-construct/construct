// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Matrix m.room.history_visibility"
};

static bool
_visible_(const m::event &event,
          const m::user::id &user_id,
          const m::room &room,
          const string_view &history_visibility)
{
	char membership_buf[32];
	const string_view membership
	{
		room.membership(membership_buf, user_id)
	};

	if(membership == "join")
		return true;

	if(history_visibility == "joined")
		return false;

	if(history_visibility == "invited")
		return membership == "invite";

	assert(history_visibility == "shared");
	const m::room present{room.room_id};
	return present.membership(user_id, "join");
}

static bool
_visible_(const m::event &event,
          const m::node::id &node_id,
          const m::room &room,
          const string_view &history_visibility)
{
	return true;
}

static bool
_visible(const m::event &event,
         const string_view &mxid,
         const m::room &room,
         const string_view &history_visibility)
{
	if(history_visibility == "world_readable")
		return true;

	if(empty(mxid))
		return false;

	switch(m::sigil(mxid))
	{
		case m::id::USER:
			return _visible_(event, m::user::id{mxid}, room, history_visibility);

		case m::id::NODE:
			return _visible_(event, m::node::id{mxid}, room, history_visibility);

		default: throw m::UNSUPPORTED
		{
			"Cannot determine visibility for '%s' mxids",
			reflect(m::sigil(mxid))
		};
	}
}

extern "C" bool
visible(const m::event &event,
        const string_view &mxid)
{
	const m::room room
	{
		at<"room_id"_>(event), at<"event_id"_>(event)
	};

	static const m::event::fetch::opts fopts
	{
		m::event::keys::include{"content"}
	};

	const m::room::state state
	{
		room, &fopts
	};

	bool ret{false};
	const bool has_state_event
	{
		state.get(std::nothrow, "m.room.history_visibility", "", [&]
		(const m::event &event)
		{
			const json::object &content
			{
				json::get<"content"_>(event)
			};

			const string_view &history_visibility
			{
				unquote(content.get("history_visibility", "shared"))
			};

			ret = _visible(event, mxid, room, history_visibility);
		})
	};

	return !has_state_event?
		_visible(event, mxid, room, "shared"):
		ret;
}

static void
_changed_visibility(const m::event &event)
{
	log::info
	{
		"Changed visibility of %s to %s by %s => %s",
		json::get<"room_id"_>(event),
		json::get<"content"_>(event).get("history_visibility"),
		json::get<"sender"_>(event),
		json::get<"event_id"_>(event)
	};
}

const m::hookfn<>
_changed_visibility_hookfn
{
	_changed_visibility,
	{
		{ "_site",    "vm.notify"                  },
		{ "type",     "m.room.history_visibility"  },
	}
};
