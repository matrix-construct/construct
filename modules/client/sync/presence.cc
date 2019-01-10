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
	"Client Sync :Presence"
};

namespace ircd::m::sync
{
	static void presence_polylog_events(data &);
	static void presence_polylog(data &);

	static void presence_linear_events(data &);
	static void presence_linear(data &);

	extern item presence;
}

decltype(ircd::m::sync::presence)
ircd::m::sync::presence
{
	"presence",
	presence_polylog,
	presence_linear,
};

void
ircd::m::sync::presence_linear(data &data)
{
	return;

	assert(data.event);
	const m::event &event
	{
		*data.event
	};

	if(json::get<"type"_>(event) != "ircd.presence")
		return;

	if(json::get<"sender"_>(event) != m::me.user_id)
		return;

	// sender
	json::stack::member
	{
		data.out, "sender", unquote(at<"content"_>(event).get("user_id"))
	};

	// type
	json::stack::member
	{
		data.out, "type", json::value{"m.presence"}
	};

	// content
	json::stack::member
	{
		data.out, "content", at<"content"_>(event)
	};

	return;
}

void
ircd::m::sync::presence_polylog(data &data)
{
	json::stack::object object
	{
		data.out
	};

	presence_polylog_events(data);
}

void
ircd::m::sync::presence_polylog_events(data &data)
{
	json::stack::array array
	{
		data.out, "events"
	};

	ctx::mutex mutex;
	const auto closure{[&data, &array, &mutex]
	(const json::object &event)
	{
		// Lock the json::stack for the append operations. This mutex will only be
		// contended during a json::stack flush to the client; not during database
		// queries leading to this.
		const std::lock_guard<decltype(mutex)> l{mutex};

		data.commit();
		json::stack::object object
		{
			array
		};

		// sender
		json::stack::member
		{
			object, "sender", unquote(event.get("user_id"))
		};

		// type
		json::stack::member
		{
			object, "type", json::value{"m.presence"}
		};

		// content
		json::stack::member
		{
			object, "content", event
		};
	}};

	//TODO: conf
	static const size_t fibers(32);
	std::array<string_view, fibers> q;
	std::array<char[128], fibers> buf; //TODO: X
	ctx::parallel<string_view> parallel
	{
		m::sync::pool, q, [&data, &closure](const auto &user_id)
		{
			const m::user user{user_id};
			const m::user::room user_room{user};
			//TODO: can't check event_idx cuz only closed presence content
			if(head_idx(std::nothrow, user_room) >= data.range.first)
				m::presence::get(std::nothrow, user, closure);
		}
	};

	const m::user::mitsein mitsein{data.user};
	mitsein.for_each("join", [&parallel, &q, &buf]
	(const m::user &user)
	{
		q[parallel.snd] = strlcpy(buf[parallel.snd], user.user_id);
		parallel();
	});
}
