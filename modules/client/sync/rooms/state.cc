// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::sync
{
	static void room_state_append(data &, json::stack::array &, const m::event &, const m::event::idx &);

	static bool room_state_polylog_events(data &);
	static bool _room_state_polylog(data &);
	static bool room_state_polylog(data &);
	static bool room_invite_state_polylog(data &);

	static bool room_state_linear_events(data &);
	static bool room_state_linear(data &);

	extern const event::keys::include _default_keys;
	extern event::fetch::opts _default_fopts;

	extern item room_invite_state;
	extern item room_state;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client Sync :Room State", []
	{
		ircd::m::sync::_default_fopts.query_json_force = true;
	}
};

decltype(ircd::m::sync::room_state)
ircd::m::sync::room_state
{
	"rooms.state",
	room_state_polylog,
	room_state_linear,
	{
		{ "phased", true },
	}
};

decltype(ircd::m::sync::room_invite_state)
ircd::m::sync::room_invite_state
{
	"rooms.invite_state",
	room_invite_state_polylog,
	room_state_linear
};

decltype(ircd::m::sync::_default_keys)
ircd::m::sync::_default_keys
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

decltype(ircd::m::sync::_default_fopts)
ircd::m::sync::_default_fopts
{
	_default_keys
};

bool
ircd::m::sync::room_state_linear(data &data)
{
	// if since token is non-zero, any events in the range are
	// included in the timeline array and not the state array.
	if(data.range.first)
		return false;

	if(!data.event_idx)
		return false;

	if(!data.membership)
		return false;

	if(!data.room)
		return false;

	assert(data.event);
	if(!json::get<"state_key"_>(*data.event))
		return false;

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

	json::stack::object state
	{
		*data.out, "state"
	};

	json::stack::array array
	{
		*data.out, "events"
	};

	room_state_append(data, array, *data.event, data.event_idx);
	return true;
}

bool
ircd::m::sync::room_state_polylog(data &data)
{
	if(data.membership == "invite")
		return false;

	return _room_state_polylog(data);
}

bool
ircd::m::sync::room_invite_state_polylog(data &data)
{
	if(data.membership != "invite")
		return false;

	return _room_state_polylog(data);
}

bool
ircd::m::sync::_room_state_polylog(data &data)
{
	if(!apropos(data, data.room_head))
		return false;

	return room_state_polylog_events(data);
}

bool
ircd::m::sync::room_state_polylog_events(data &data)
{
	const m::room &room{*data.room};
	const m::room::state state{room};

	json::stack::array array
	{
		*data.out, "events"
	};

	bool ret{false};
	if(data.phased && data.range.first == 0)
	{
		data.room->get(std::nothrow, "m.room.create", "", [&]
		(const m::event &event)
		{
			room_state_append(data, array, event, index(event));
			ret = true;
		});

		data.room->get(std::nothrow, "m.room.canonical_alias", "", [&]
		(const m::event &event)
		{
			room_state_append(data, array, event, index(event));
		});

		data.room->get(std::nothrow, "m.room.name", "", [&]
		(const m::event &event)
		{
			room_state_append(data, array, event, index(event));
		});

		return ret;
	}

	ctx::mutex mutex;
	const event::closure_idx each_idx{[&data, &array, &mutex, &ret]
	(const m::event::idx event_idx)
	{
		const event::fetch event
		{
			event_idx, std::nothrow, _default_fopts
		};

		//assert(event.valid);
		if(unlikely(!event.valid))
		{
			assert(data.room);
			log::error
			{
				log, "Failed to fetch event idx:%lu in room %s state.",
				event_idx,
				string_view{data.room->room_id}
			};

			return;
		}

		const std::lock_guard lock{mutex};
		room_state_append(data, array, event, event_idx);
		ret = true;
	}};

	//TODO: conf
	std::array<event::idx, 64> md;
	ctx::parallel<event::idx> parallel
	{
		m::sync::pool, md, each_idx
	};

	state.for_each([&data, &parallel]
	(const m::event::idx &event_idx)
	{
		if(apropos(data, event_idx))
			parallel(event_idx);
	});

	parallel.wait_done();
	return ret;
}

void
ircd::m::sync::room_state_append(data &data,
                                 json::stack::array &events,
                                 const m::event &event,
                                 const m::event::idx &event_idx)
{
	m::event_append_opts opts;
	opts.event_idx = &event_idx;
	opts.user_id = &data.user.user_id;
	opts.user_room = &data.user_room;
	opts.query_txnid = false;
	m::append(events, event, opts);
}
