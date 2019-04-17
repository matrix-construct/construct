// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Federation :Request a prototype for creating a leave event."
};

const string_view
make_leave_description
{R"(

Sends a partial event to the remote with enough information for them to
create a leave event 'in the blind' for one of their users.

)"};

resource
make_leave_resource
{
	"/_matrix/federation/v1/make_leave/",
	{
		make_leave_description,
		resource::DIRECTORY
	}
};

resource::response
get__make_leave(client &client,
                const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"room_id path parameter required"
		};

	m::room::id::buf room_id
	{
		url::decode(room_id, request.parv[0])
	};

	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"user_id path parameter required"
		};

	m::user::id::buf user_id
	{
		url::decode(user_id, request.parv[1])
	};

	if(user_id.host() != request.node_id.host())
		throw m::ACCESS_DENIED
		{
			"You are not permitted to spoof users on other hosts."
		};

	const m::room room
	{
		room_id
	};

	char membuf[32];
	const string_view membership
	{
		room.membership(membuf, user_id)
	};

	if(membership != "join" && membership != "invite")
		throw m::ACCESS_DENIED
		{
			"You are not permitted to leave the room with membership '%s'",
			membership
		};

	const unique_buffer<mutable_buffer> buf
	{
		8_KiB
	};

	json::stack out{buf};
	json::stack::object top{out};
	json::stack::object event
	{
		top, "event"
	};

	{
		const m::room::auth auth{room};
		json::stack::array auth_events
		{
			event, "auth_events"
		};

		static const string_view types[]
		{
			"m.room.create",
			"m.room.join_rules",
			"m.room.power_levels",
			"m.room.member",
		};

		auth.make_refs(auth_events, types, user_id);
	}

	json::stack::member
	{
		event, "content", R"({"membership":"leave"})"
	};

	json::stack::member
	{
		event, "depth", json::value(m::depth(room) + 1L)
	};

	json::stack::member
	{
		event, "origin", request.origin
	};

	json::stack::member
	{
		event, "origin_server_ts", json::value(time<milliseconds>())
	};

	{
		const m::room::head head{room};
		json::stack::array prev_events
		{
			event, "prev_events"
		};

		head.make_refs(prev_events, 32, true);
	}

	json::stack::member
	{
		event, "room_id", room.room_id
	};

	json::stack::member
	{
		event, "sender", user_id
	};

	json::stack::member
	{
		event, "state_key", user_id
	};

	json::stack::member
	{
		event, "type", "m.room.member"
	};

	event.~object();
	top.~object();
	return resource::response
	{
		client, json::object
		{
			out.completed()
		}
	};
}

resource::method
method_get
{
	make_leave_resource, "GET", get__make_leave,
	{
		method_get.VERIFY_ORIGIN
	}
};
