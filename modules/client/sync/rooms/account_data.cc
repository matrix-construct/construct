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
	static bool room_account_data_events_polylog(data &);
	static bool room_account_data_polylog(data &);
	extern item room_account_data;
}

decltype(ircd::m::sync::room_account_data)
ircd::m::sync::room_account_data
{
	"rooms.account_data",
	room_account_data_polylog
};

bool
ircd::m::sync::room_account_data_polylog(data &data)
{
	json::stack::object object
	{
		data.out
	};

	room_account_data_events_polylog(data);
	return true;
}

bool
ircd::m::sync::room_account_data_events_polylog(data &data)
{
	json::stack::array array
	{
		data.out, "events"
	};

	const m::room::state state
	{
		data.user_room
	};

	char typebuf[288]; //TODO: room_account_data_typebuf_size
	const auto type
	{
		m::user::_account_data_type(typebuf, data.room->room_id)
	};

	state.for_each(type, [&data, &array]
	(const m::event &event)
	{
		if(!apropos(data, event))
			return;

		json::stack::object object
		{
			data.out
		};

		json::stack::member
		{
			object, "type", at<"state_key"_>(event)
		};

		json::stack::member
		{
			object, "content", at<"content"_>(event)
		};
	});

	return true;
}
