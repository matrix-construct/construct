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
	"Client Sync :Account Data"
};

namespace ircd::m::sync
{
	static bool account_data_(data &, const m::event &, const m::event::idx &);
	static bool account_data_polylog(data &);
	static bool account_data_linear(data &);

	extern item account_data;
}

decltype(ircd::m::sync::account_data)
ircd::m::sync::account_data
{
	"account_data",
	account_data_polylog,
	account_data_linear
};

bool
ircd::m::sync::account_data_linear(data &data)
{
	if(!data.event_idx)
		return false;

	assert(data.event);
	json::stack::array array
	{
		*data.out, "events"
	};

	return account_data_(data, *data.event, data.event_idx);
}

bool
ircd::m::sync::account_data_polylog(data &data)
{
	json::stack::array array
	{
		*data.out, "events"
	};

	static const m::event::fetch::opts fopts
	{
		m::event::keys::include {"event_id", "state_key", "content"}
	};

	const m::room::state state
	{
		data.user_room, &fopts
	};

	bool ret{false};
	state.for_each("ircd.account_data", [&data, &array, &ret]
	(const m::event &event)
	{
		ret |= account_data_(data, event, index(event));
	});

	return ret;
}

bool
ircd::m::sync::account_data_(data &data,
                             const m::event &event,
                             const m::event::idx &event_idx)
{
	if(!apropos(data, event_idx))
		return false;

	if(json::get<"type"_>(event) != "ircd.account_data")
		return false;

	if(json::get<"room_id"_>(event) != data.user_room.room_id)
		return false;

	// Each account_data event is an object in the events array
	json::stack::object object
	{
		*data.out
	};

	// type
	json::stack::member
	{
		*data.out, "type", at<"state_key"_>(event)
	};

	// content
	json::stack::member
	{
		*data.out, "content", at<"content"_>(event)
	};

	return true;
}
