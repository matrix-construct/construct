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
	static bool _room_timeline_append(data &, json::stack::array &, const m::event::idx &, const m::event &);
	static event::id::buf _room_timeline_polylog_events(data &, const m::room &, bool &, bool &);
	static bool room_timeline_polylog(data &);

	static bool _room_timeline_linear_command(data &);
	static bool room_timeline_linear(data &);

	extern conf::item<size_t> limit_default;
	extern conf::item<size_t> limit_initial_default;
	extern conf::item<int64_t> reflow_depth;
	extern item room_timeline;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client Sync :Room Timeline"
};

decltype(ircd::m::sync::room_timeline)
ircd::m::sync::room_timeline
{
	"rooms.timeline",
	room_timeline_polylog,
	room_timeline_linear
};

decltype(ircd::m::sync::reflow_depth)
ircd::m::sync::reflow_depth
{
	{ "name",     "ircd.client.sync.rooms.timeline.reflow.depth" },
	{ "default",  0L                                             },
};

decltype(ircd::m::sync::limit_default)
ircd::m::sync::limit_default
{
	{ "name",     "ircd.client.sync.rooms.timeline.limit.default" },
	{ "default",  10L                                             },
};

decltype(ircd::m::sync::limit_initial_default)
ircd::m::sync::limit_initial_default
{
	{ "name",     "ircd.client.sync.rooms.timeline.limit_initial.default" },
	{ "default",  1L                                                      },
};

bool
ircd::m::sync::room_timeline_linear(data &data)
{
	if(!data.event_idx)
		return false;

	if(!data.room)
		return false;

	if(!data.membership && *data.room != data.user_room)
		return false;

	assert(data.event);
	const bool command
	{
		*data.room == data.user_room &&
		startswith(json::get<"type"_>(*data.event), "ircd.cmd") &&
		(json::get<"sender"_>(*data.event) == me() ||
		 json::get<"sender"_>(*data.event) == data.user.user_id)
	};

	json::stack::object rooms
	{
		*data.out, "rooms"
	};

	if(command)
		return _room_timeline_linear_command(data);

	// User's room conditions must be satisfied before here.
	if(*data.room == data.user_room)
		return false;

	const ssize_t &viewport_size
	{
		room::events::viewport_size
	};

	assert(data.room_depth >= json::get<"depth"_>(*data.event));
	const auto sounding
	{
		data.room_depth - json::get<"depth"_>(*data.event)
	};

	assert(sounding >= 0);
	const bool viewport_visible
	{
		viewport_size <= 0
		|| sounding < viewport_size
	};

	const bool is_own_membership
	{
		json::get<"type"_>(*data.event) == "m.room.member"
		&& json::get<"state_key"_>(*data.event) == data.user.user_id
	};

	const bool is_own_join
	{
		is_own_membership
		&& data.membership == "join"
	};

	const auto last_membership_state_idx
	{
		is_own_join?
			m::room::state::prev(data.event_idx):
			0UL
	};

	char last_membership_buf[room::MEMBERSHIP_MAX_SIZE];
	const auto &last_membership
	{
		last_membership_state_idx?
			m::membership(last_membership_buf, last_membership_state_idx):
			string_view{}
	};

	const bool is_own_rejoin
	{
		is_own_join
		&& last_membership
	};

	const bool is_invite_accept
	{
		is_own_rejoin
		&& last_membership == "invite"
	};

	// Conditions to not synchronize this event to the client, at least
	// for here and now...
	const bool skip
	{
		false
		|| !viewport_visible
		|| (is_own_join && data.reflow_full_state)
	};

	// Conditions to redraw the timeline (limited=true).
	const bool reflow
	{
		false
		|| is_own_join
		|| (int64_t(reflow_depth) > 0 && sounding >= int64_t(reflow_depth))
	};

	if(skip)
		return false;

	json::stack::object membership_
	{
		*data.out, data.membership
	};

	json::stack::object room_
	{
		*data.out, data.room->room_id
	};

	json::stack::object timeline
	{
		*data.out, "timeline"
	};

	if(reflow)
	{
		const auto prev_batch
		{
			m::event_id(std::nothrow, data.room_head)
		};

		json::stack::member
		{
			timeline, "limited", json::value{true}
		};

		json::stack::member
		{
			timeline, "prev_batch", json::value
			{
				prev_batch, json::STRING
			}
		};
	}

	json::stack::array array
	{
		*data.out, "events"
	};

	return _room_timeline_append(data, array, data.event_idx, *data.event);
}

bool
ircd::m::sync::_room_timeline_linear_command(data &data)
{
	const m::room &room
	{
		unquote(json::get<"content"_>(*data.event).get("room_id"))
	};

	const scope_restore _room
	{
		data.room, &room
	};

	const scope_restore _membership
	{
		data.membership, "join"_sv
	};

	json::stack::object membership_
	{
		*data.out, data.membership
	};

	json::stack::object room_
	{
		*data.out, data.room->room_id
	};

	json::stack::object timeline
	{
		*data.out, "timeline"
	};

	json::stack::array array
	{
		*data.out, "events"
	};

	m::event event{*data.event};
	json::get<"type"_>(event) = "m.room.message";
	json::get<"room_id"_>(event) = room.room_id;
	const scope_restore _event
	{
		data.event, &event
	};

	return _room_timeline_append(data, array, data.event_idx, event);
}

bool
ircd::m::sync::room_timeline_polylog(data &data)
{
	if(!apropos(data, data.room_head))
		return false;

	// events
	assert(data.room);
	bool limited{true}, ret{false};
	m::event::id::buf prev
	{
		_room_timeline_polylog_events(data, *data.room, limited, ret)
	};

	// limited
	if(ret)
		json::stack::member
		{
			*data.out, "limited", json::value{limited}
		};

	// prev_batch
	if(ret && likely(prev))
		json::stack::member
		{
			*data.out, "prev_batch", string_view{prev}
		};

	return ret;
}

ircd::m::event::id::buf
ircd::m::sync::_room_timeline_polylog_events(data &data,
                                             const m::room &room,
                                             bool &limited,
                                             bool &ret)
{
	json::stack::array array
	{
		*data.out, "events"
	};

	// messages seeks to the newest event, but the client wants the oldest
	// event first so we seek down first and then iterate back up. Due to
	// an issue with rocksdb's prefix-iteration this iterator becomes
	// toxic as soon as it becomes invalid. As a result we have to copy the
	// event_idx on the way down in case of renewing the iterator for the
	// way back. This is not a big deal but rocksdb should fix their shit.
	m::event::id::buf event_id;
	m::event::idx event_idx {data.room_head};
	m::room::events it
	{
		room
	};

	const ssize_t limit
	{
		data.phased && data.range.first == 0?
			ssize_t(limit_initial_default): // phased + initial=true
			ssize_t(limit_default)
	};

	ssize_t i(0), prefetched(0);
	for(; it && i <= limit; --it)
	{
		event_idx = it.event_idx();
		if(!i && event_idx >= data.range.second)
			continue;

		if(event_idx < data.range.first)
		{
			limited = false;
			break;
		}

		prefetched += limit > 1 && m::prefetch(event_idx);
		++i;
	}

	if(i > 1 && !it)
		it.seek(event_idx);

	if(i > 1 && it)
		--i, ++it;

	if(i > 0 && it)
		for(++it; i > 0 && it; --i, ++it)
		{
			const m::event &event
			{
				*it
			};

			ret |= _room_timeline_append(data, array, it.event_idx(), event);
		}

	assert(i >= 0);
	event_idx &= boolmask<event::idx>(ret);
	return m::event_id(std::nothrow, event_idx);
}

bool
ircd::m::sync::_room_timeline_append(data &data,
                                     json::stack::array &events,
                                     const m::event::idx &event_idx,
                                     const m::event &event)
{
	m::event::append::opts opts;
	opts.event_idx = &event_idx;
	opts.client_txnid = &data.client_txnid;
	opts.user_id = &data.user.user_id;
	opts.user_room = &data.user_room;
	opts.room_depth = &data.room_depth;
	return m::event::append(events, event, opts);
}
