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
	"Federation :Send join event"
};

const string_view
send_join_description
{R"(

Inject a join event into a room originating from a server without any joined
users in that room.

)"};

resource
send_join_resource
{
	"/_matrix/federation/v1/send_join/",
	{
		send_join_description,
		resource::DIRECTORY
	}
};

resource::response
put__send_join(client &client,
               const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"room_id path parameter required"
		};

	m::room::id::buf room_id
	{
		url::decode(request.parv[0], room_id)
	};

	if(!my_host(room_id.host()))
		throw m::error
		{
			http::FORBIDDEN, "M_INVALID_ROOM_ID",
			"Can only send_join for rooms on my host '%s'",
			my_host()
		};

	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"event_id path parameter required"
		};

	m::event::id::buf event_id
	{
		url::decode(request.parv[1], event_id)
	};

	const m::event event
	{
		request
	};

	if(at<"event_id"_>(event) != event_id)
		throw m::error
		{
			http::NOT_MODIFIED, "M_MISMATCH_EVENT_ID",
			"ID of event in request body does not match path parameter."
		};

	if(at<"room_id"_>(event) != room_id)
		throw m::error
		{
			http::NOT_MODIFIED, "M_MISMATCH_ROOM_ID",
			"ID of room in request body does not match path parameter."
		};

	if(json::get<"type"_>(event) != "m.room.member")
		throw m::error
		{
			http::NOT_MODIFIED, "M_INVALID_TYPE",
			"Event type must be m.room.member"
		};

	if(json::get<"membership"_>(event) != "join")
		throw m::error
		{
			http::NOT_MODIFIED, "M_INVALID_MEMBERSHIP",
			"Event membership state must be 'join'."
		};

	if(unquote(json::get<"content"_>(event).get("membership")) != "join")
		throw m::error
		{
			http::NOT_MODIFIED, "M_INVALID_CONTENT_MEMBERSHIP",
			"Event content.membership state must be 'join'."
		};

	if(json::get<"origin"_>(event) != request.origin)
		throw m::error
		{
			http::NOT_MODIFIED, "M_MISMATCH_ORIGIN",
			"Event origin must be you."
		};

	m::vm::opts vmopts;
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	m::vm::eval eval
	{
		event, vmopts
	};

	// Riotapse wants the state before this join event. This means we have
	// to iterate the state btree which is slower than the sequential
	// present-state table :/
	const m::event::prev prevs{event};
	const m::room::state state
	{
		m::room
		{
			room_id, prevs.prev_event(0)
		}
	};

	//TODO: direct to socket
	const unique_buffer<mutable_buffer> buf{2_MiB}; //TODO: XXX
	json::stack out{buf};
	{
		json::stack::array top
		{
			out
		};

		// First element is the 200
		top.append(json::value(200L));

		// Second element is the object
		json::stack::object data{top};
		{
			json::stack::member auth_chain_m
			{
				data, "auth_chain"
			};

			json::stack::array auth_chain_a
			{
				auth_chain_m
			};
		}

		{
			json::stack::member state_m
			{
				data, "state"
			};

			json::stack::array state_a
			{
				state_m
			};

			state.for_each([&state_a]
			(const m::event &event)
			{
				state_a.append(event);
			});
		}
	}

	const json::array response
	{
		out.completed()
	};

	return resource::response
	{
		client, response
	};
}

resource::method
method_put
{
	send_join_resource, "PUT", put__send_join,
	{
		method_put.VERIFY_ORIGIN
	}
};
