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
	if(!data.event || !data.event_idx)
		return false;

	const m::event &event{*data.event};
	if(json::get<"room_id"_>(event) != data.user_room.room_id)
		return false;

	char typebuf[m::user::room_account_data::typebuf_size];
	const auto type
	{
		m::user::room_account_data::_type(typebuf, data.room->room_id)
	};

	if(json::get<"type"_>(event) != type)
		return false;

	json::stack::array array
	{
		*data.out, "events"
	};

	return room_account_data_polylog_events_event(data, event);
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
	char typebuf[m::user::room_account_data::typebuf_size];
	const auto type
	{
		m::user::room_account_data::_type(typebuf, data.room->room_id)
	};

	bool ret{false};
	data.user_state.for_each(type, [&data, &ret]
	(const m::event::idx &event_idx)
	{
		if(!apropos(data, event_idx))
			return;

		static const event::fetch::opts fopts
		{
			event::keys::include {"state_key", "content"}
		};

		const m::event::fetch event
		{
			event_idx, std::nothrow, fopts
		};

		if(!event.valid)
			return;

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
