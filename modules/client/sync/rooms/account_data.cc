// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::mapi::header
IRCD_MODULE
{
	"Client Sync :Room Account Data"
};

namespace ircd::m::sync
{
	static bool room_account_data_polylog_events_event(data &, const m::event &);
	static bool room_account_data_polylog_events(data &);
	static bool room_account_data_polylog(data &);
	static bool room_account_data_linear(data &);

	extern item room_account_data;
}

decltype(ircd::m::sync::room_account_data)
ircd::m::sync::room_account_data
{
	"rooms.account_data",
	room_account_data_polylog,
	room_account_data_linear
};

bool
ircd::m::sync::room_account_data_linear(data &data)
{
	return false;
}

bool
ircd::m::sync::room_account_data_polylog(data &data)
{
	return room_account_data_polylog_events(data);
}

bool
ircd::m::sync::room_account_data_polylog_events(data &data)
{
	json::stack::array array
	{
		*data.out, "events"
	};

	assert(data.room);
	char typebuf[288]; //TODO: room_account_data_typebuf_size
	const auto type
	{
		m::user::room_account_data::_type(typebuf, data.room->room_id)
	};

	static const m::event::fetch::opts &fopts
	{
		m::event::keys::include {"event_id", "state_key", "content"}
	};

	const m::room::state state
	{
		data.user_room, &fopts
	};

	bool ret{false};
	state.for_each(type, [&data, &ret]
	(const m::event &event)
	{
		if(apropos(data, event))
			ret |= room_account_data_polylog_events_event(data, event);
	});

	return ret;
}

bool
ircd::m::sync::room_account_data_polylog_events_event(data &data,
                                                      const m::event &event)
{
	json::stack::object object
	{
		*data.out
	};

	json::stack::member
	{
		object, "type", at<"state_key"_>(event)
	};

	json::stack::member
	{
		object, "content", at<"content"_>(event)
	};

	return true;
}
