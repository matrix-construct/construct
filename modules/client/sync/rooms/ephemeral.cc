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
	"Client Sync :Rooms Ephemeral"
};

namespace ircd::m::sync
{
	static bool rooms_ephemeral_events_polylog(data &);
	static bool rooms_ephemeral_polylog(data &);
	static bool rooms_ephemeral_linear(data &);
	extern item rooms_ephemeral;
}

decltype(ircd::m::sync::rooms_ephemeral)
ircd::m::sync::rooms_ephemeral
{
	"rooms.ephemeral",
	rooms_ephemeral_polylog,
	rooms_ephemeral_linear
};

bool
ircd::m::sync::rooms_ephemeral_linear(data &data)
{
	assert(data.event);
	if(json::get<"event_id"_>(*data.event))
		return false;

	json::stack::array array
	{
		*data.out, "events"
	};

	bool ret{false};
	m::sync::for_each("rooms.ephemeral", [&data, &ret]
	(item &item)
	{
		json::stack::checkpoint checkpoint
		{
			*data.out
		};

		if(item.linear(data))
			ret = true;
		else
			checkpoint.rollback();

		return true;
	});

	return ret;
}

bool
ircd::m::sync::rooms_ephemeral_polylog(data &data)
{
	return rooms_ephemeral_events_polylog(data);
}

bool
ircd::m::sync::rooms_ephemeral_events_polylog(data &data)
{
	json::stack::array array
	{
		*data.out, "events"
	};

	bool ret{false};
	m::sync::for_each("rooms.ephemeral", [&data, &ret]
	(item &item)
	{
		json::stack::checkpoint checkpoint
		{
			*data.out
		};

		if(item.polylog(data))
			ret = true;
		else
			checkpoint.rollback();

		return true;
	});

	return ret;
}
