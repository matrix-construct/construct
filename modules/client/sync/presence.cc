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
	static bool presence_polylog(data &);
	static bool presence_linear(data &);

	extern item presence;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client Sync :Presence"
};

decltype(ircd::m::sync::presence)
ircd::m::sync::presence
{
	"presence",
	presence_polylog,
	presence_linear,
};

bool
ircd::m::sync::presence_linear(data &data)
{
	if(!data.event_idx)
		return false;

	assert(data.event);
	const m::event &event{*data.event};
	if(json::get<"type"_>(event) != "ircd.presence")
		return false;

	if(!my_host(json::get<"origin"_>(event)))
		return false;

	const json::object &content
	{
		at<"content"_>(event)
	};

	const json::string &sender
	{
		content["user_id"]
	};

	const m::user::mitsein mitsein
	{
		data.user
	};

	assert(sender);
	if(!mitsein.has(m::user::id(sender), "join"))
		return false;

	json::stack::object presence
	{
		*data.out, "presence"
	};

	json::stack::array array
	{
		*data.out, "events"
	};

	json::stack::object object
	{
		*data.out
	};

	// sender
	json::stack::member
	{
		*data.out, "sender", sender
	};

	// type
	json::stack::member
	{
		*data.out, "type", json::value{"m.presence"}
	};

	// content
	json::stack::member
	{
		*data.out, "content", at<"content"_>(event)
	};

	return true;
}

bool
ircd::m::sync::presence_polylog(data &data)
{
	json::stack::array array
	{
		*data.out, "events"
	};

	bool ret{false};
	ctx::mutex mutex;
	const auto append_event{[&data, &array, &mutex, &ret]
	(const json::object &event)
	{
		// Conditions to not send; we don't send for offline users
		// on initial sync, unless they have a message set.
		const bool skip
		{
			data.range.first == 0
			&& unquote(event.get("presence")) == "offline"
			&& !event.has("status_msg")
		};

		if(skip)
			return;

		const json::string &user_id
		{
			event.get("user_id")
		};

		if(!valid(m::id::USER, user_id))
			return;

		// Lock the json::stack for the append operations. This mutex will only be
		// contended during a json::stack flush to the client; not during database
		// queries leading to this.
		const std::lock_guard l{mutex};
		ret = true;

		json::stack::object object
		{
			array
		};

		// sender
		json::stack::member
		{
			object, "sender", user_id
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

	// Setup for concurrentization.
	static const size_t fibers(64);
	sync::pool.min(fibers);
	ctx::concurrent<std::string> concurrent
	{
		sync::pool, [&data, &append_event](std::string user_id)
		{
			const event::idx event_idx
			{
				m::presence::get(std::nothrow, m::user::id{user_id})
			};

			if(!apropos(data, event_idx))
				return;

			m::get(std::nothrow, event_idx, "content", append_event);
		}
	};

	// Iterate all of the users visible to our user in joined rooms.
	const m::user::mitsein mitsein{data.user};
	mitsein.for_each("join", [&concurrent]
	(const m::user &user)
	{
		concurrent(std::string(user.user_id));
		return true;
	});

	const ctx::uninterruptible ui;
	concurrent.wait();
	return ret;
}
