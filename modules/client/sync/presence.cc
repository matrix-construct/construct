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
	static bool presence_polylog(data &);
	static bool presence_linear(data &);
	extern item presence;
}

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
	if(!data.event)
		return false;

	if(json::get<"type"_>(*data.event) != "ircd.presence")
		return false;

	json::stack::object object
	{
		*data.array
	};

	// sender
	json::stack::member
	{
		object, "sender", unquote(at<"content"_>(*data.event).get("user_id"))
	};

	// type
	json::stack::member
	{
		object, "type", json::value{"m.presence"}
	};

	// content
	json::stack::member
	{
		object, "content", at<"content"_>(*data.event)
	};

	return true;
}

bool
ircd::m::sync::presence_polylog(data &data)
{
	json::stack::object out{*data.member};
	json::stack::member member{out, "events"};
	json::stack::array array{member};
	const m::user::mitsein mitsein
	{
		data.user
	};

	ctx::mutex mutex;
	const auto closure{[&data, &array, &mutex]
	(const json::object &event)
	{
		// Lock the json::stack for the append operations. This mutex will only be
		// contended during a json::stack flush to the client; not during database
		// queries leading to this.
		const std::lock_guard<decltype(mutex)> l{mutex};
		json::stack::object object{array};

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

	const auto each_user{[&data, &closure]
	(const m::user::id &user_id)
	{
		const m::user user{user_id};
		const m::user::room user_room{user};
		//TODO: can't check event_idx cuz only closed presence content
		if(head_idx(std::nothrow, user_room) > data.since)
			m::presence::get(std::nothrow, user, closure);
	}};

	mitsein.for_each("join", each_user);
/*
	//TODO: conf
	static const size_t fibers(24);
	string_view q[fibers];
	char buf[fibers][256];
	ctx::parallel<string_view> parallel
	{
		meepool, q, each_user
	};

	const auto paraclosure{[&parallel, &q, &buf]
	(const m::user &u)
	{
		assert(parallel.snd < fibers);
		strlcpy(buf[parallel.snd], string_view{u.user_id});
		q[parallel.snd] = buf[parallel.snd];
		parallel();
	}};
*/
//	mitsein.for_each("join", paraclosure);
	return true;
}
