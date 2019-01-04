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
	"Client Sync :Room Timeline"
};

namespace ircd::m::sync
{
	static event::id::buf _room_timeline_events(data &, json::stack::array &, const m::room &, bool &);
	static bool room_timeline_polylog(data &);
	extern item room_timeline;
}

decltype(ircd::m::sync::room_timeline)
ircd::m::sync::room_timeline
{
	"rooms.$membership.$room_id.timeline",
	room_timeline_polylog
};

bool
ircd::m::sync::room_timeline_polylog(data &data)
{
	json::stack::object out{*data.member};

	// events
	bool limited{false};
	m::event::id::buf prev;
	{
		json::stack::member member{out, "events"};
		json::stack::array array{member};
		prev = _room_timeline_events(data, array, *data.room, limited);
	}

	// prev_batch
	json::stack::member
	{
		out, "prev_batch", string_view{prev}
	};

	// limited
	json::stack::member
	{
		out, "limited", json::value{limited}
	};

	return true;
}

ircd::m::event::id::buf
ircd::m::sync::_room_timeline_events(data &data,
                                     json::stack::array &out,
                                     const m::room &room,
                                     bool &limited)
{
	static const m::event::fetch::opts fopts
	{
		m::event::keys::include
		{
			"content",
			"depth",
			"event_id",
			"origin_server_ts",
			"prev_events",
			"redacts",
			"room_id",
			"sender",
			"state_key",
			"type",
		},
	};

	// messages seeks to the newest event, but the client wants the oldest
	// event first so we seek down first and then iterate back up. Due to
	// an issue with rocksdb's prefix-iteration this iterator becomes
	// toxic as soon as it becomes invalid. As a result we have to copy the
	// event_id on the way down in case of renewing the iterator for the
	// way back. This is not a big deal but rocksdb should fix their shit.
	ssize_t i(0);
	m::event::id::buf event_id;
	m::room::messages it
	{
		room, &fopts
	};

	for(; it && i < 10; --it, ++i)
	{
		event_id = it.event_id();
		if(it.event_idx() < data.since)
			break;

		if(it.event_idx() > data.current)
			break;
	}

	limited = i >= 10;
	if(i > 0)
		data.commit();

	if(i > 0 && !it)
		it.seek(event_id);

	if(i > 0 && it)
	{
		const m::event &event{*it};
		data.state_at = at<"depth"_>(event);
	}

	if(i > 0)
		for(; it && i > -1; ++it, --i)
			out.append(*it);

	return event_id;
}
