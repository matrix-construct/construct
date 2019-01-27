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
	static event::id::buf _room_timeline_polylog_events(data &, const m::room &, bool &);
	static void room_timeline_polylog(data &);

	static event::id::buf _room_timeline_linear_events(data &, const m::room &, bool &);
	static void room_timeline_linear(data &);

	extern const event::keys::include default_keys;
	extern item room_timeline;
}

decltype(ircd::m::sync::room_timeline)
ircd::m::sync::room_timeline
{
	"rooms.timeline",
	room_timeline_polylog,
	room_timeline_linear,
};

decltype(ircd::m::sync::default_keys)
ircd::m::sync::default_keys
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
};

void
ircd::m::sync::room_timeline_linear(data &data)
{
	return;

	json::stack::object object
	{
		data.out
	};

	m::room room;

	// events
	bool limited{false};
	m::event::id::buf prev
	{
		_room_timeline_linear_events(data, room, limited)
	};

	// prev_batch
	json::stack::member
	{
		object, "prev_batch", string_view{prev}
	};

	// limited
	json::stack::member
	{
		object, "limited", json::value{limited}
	};

	return;
}

ircd::m::event::id::buf
ircd::m::sync::_room_timeline_linear_events(data &data,
                                            const m::room &room,
                                            bool &limited)
{
	json::stack::array array
	{
		data.out, "events"
	};

	return {};
}

void
ircd::m::sync::room_timeline_polylog(data &data)
{
	json::stack::object object
	{
		data.out
	};

	const auto head_idx
	{
		m::head_idx(std::nothrow, *data.room)
	};

	if(!apropos(data, head_idx))
		return;

	// events
	bool limited{false};
	m::event::id::buf prev
	{
		_room_timeline_polylog_events(data, *data.room, limited)
	};

	// prev_batch
	json::stack::member
	{
		object, "prev_batch", string_view{prev}
	};

	// limited
	json::stack::member
	{
		object, "limited", json::value{limited}
	};
}

ircd::m::event::id::buf
ircd::m::sync::_room_timeline_polylog_events(data &data,
                                             const m::room &room,
                                             bool &limited)
{
	static const event::fetch::opts fopts
	{
		default_keys
	};

	json::stack::array array
	{
		data.out, "events"
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
		if(!apropos(data, it.event_idx()))
			break;
	}

	limited = i >= 10;
	if(i > 0)
		data.commit();

	if(i > 0 && !it)
		it.seek(event_id);

	if(i > 0 && it)
	{
		data.commit();
		//const m::event &event{*it};
		//data.state_at = at<"depth"_>(event);
	}

	if(i > 0)
		for(; it && i > -1; ++it, --i)
		{
			data.commit();
			json::stack::object object
			{
				array
			};

			const m::event &event(*it);
			object.append(event);

			json::stack::object unsigned_
			{
				object, "unsigned"
			};

			json::stack::member
			{
				unsigned_, "age", json::value
				{
					long(vm::current_sequence - it.event_idx())
				}
			};
		}

	return event_id;
}
