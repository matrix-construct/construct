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
		(json::get<"sender"_>(*data.event) == m::me.user_id ||
		 json::get<"sender"_>(*data.event) == data.user.user_id)
	};

	json::stack::object rooms
	{
		*data.out, "rooms"
	};

	if(command)
		return _room_timeline_linear_command(data);

	const ssize_t &viewport_size
	{
		room::events::viewport_size
	};

	if(likely(viewport_size >= 0))
		if(json::get<"depth"_>(*data.event) + viewport_size < data.room_depth)
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

	const bool is_own_membership
	{
		json::get<"type"_>(*data.event) == "m.room.member"
		&& json::get<"state_key"_>(*data.event) == data.user.user_id
	};

	const bool is_own_join
	{
		is_own_membership && data.membership == "join"
	};

	// Branch to backfill the user's timeline before their own join event to
	// the room. This simply reuses the polylog handler as if they were
	// initial-syncing the room.
	if(is_own_join)
	{
		const auto last_membership_state_idx
		{
			m::room::state::prev(data.event_idx)
		};

		const scope_restore range_first
		{
			data.range.first, last_membership_state_idx
		};

		bool ret{false}, limited{false};
		const auto prev_batch
		{
			_room_timeline_polylog_events(data, *data.room, limited, ret)
		};

		return ret;
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
	bool limited{false}, ret{false};
	m::event::id::buf prev
	{
		_room_timeline_polylog_events(data, *data.room, limited, ret)
	};

	// limited
	json::stack::member
	{
		*data.out, "limited", json::value{limited}
	};

	// prev_batch
	if(likely(prev))
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
	m::event::idx event_idx {0};
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
			break;

		if(limit > 1)
			prefetched += m::prefetch(event_idx);

		++i;
	}

	limited = i > limit;
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

	return m::event_id(event_idx, std::nothrow);
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
