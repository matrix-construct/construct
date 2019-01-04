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
	"Client Sync :Room State"
};

namespace ircd::m::sync
{
	static bool room_state_polylog(data &);
	static bool room_state_linear(data &);
	extern item room_state;
}

decltype(ircd::m::sync::room_state)
ircd::m::sync::room_state
{
	"rooms.$membership.$room_id.state",
	room_state_polylog,
	room_state_linear
};

bool
ircd::m::sync::room_state_linear(data &data)
{
	assert(data.event);
	assert(data.room);
	assert(json::get<"room_id"_>(*data.event));

	if(!json::get<"state_key"_>(*data.event))
		return false;

	if(!data.room->membership(data.user, data.membership))
		return false;

	data.array->append(*data.event);
	return true;
}

bool
ircd::m::sync::room_state_polylog(data &data)
{
	static const m::event::keys::include default_keys
	{
		"content",
		"depth",
		"event_id",
		"origin_server_ts",
		"redacts",
		"room_id",
		"sender",
		"state_key",
		"type",
	};

	static const m::event::fetch::opts fopts
	{
		default_keys
	};

	json::stack::object out{*data.member};
	json::stack::member member
	{
		out, "events"
	};

	json::stack::array array
	{
		member
	};

	ctx::mutex mutex;
	const event::closure_idx each_idx{[&data, &array, &mutex]
	(const m::event::idx &event_idx)
	{
		assert(event_idx);
		const event::fetch event
		{
			event_idx, std::nothrow, &fopts
		};

		if(!event.valid || at<"depth"_>(event) >= int64_t(data.state_at))
			return;

		const std::lock_guard<decltype(mutex)> lock{mutex};
		array.append(event);
		data.commit();
	}};

	const event::closure_idx _each_idx{[&data, &each_idx]
	(const m::event::idx &event_idx)
	{
		assert(event_idx);
		if(event_idx >= data.since && event_idx <= data.current)
			each_idx(event_idx);
	}};

	const m::room &room{*data.room};
	const m::room::state state{room};
	state.for_each(_each_idx);
	return true;
}
