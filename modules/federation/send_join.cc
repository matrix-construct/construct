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

static void
send_join__response(client &,
                    const m::resource::request &,
                    const m::event &,
                    json::stack::object &out);

static m::resource::response
put__send_join(client &,
               const m::resource::request &);

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

m::resource
send_join_resource
{
	"/_matrix/federation/v1/send_join/",
	{
		send_join_description,
		resource::DIRECTORY
	}
};

m::resource::method
method_put
{
	send_join_resource, "PUT", put__send_join,
	{
		method_put.VERIFY_ORIGIN
	}
};

m::resource::response
put__send_join(client &client,
               const m::resource::request &request)
{
	const bool v1
	{
		request.version == "v1"
	};

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
			"event_id path parameter required"
		};

	m::event::id::buf event_id
	{
		url::decode(event_id, request.parv[1])
	};

	const m::event event
	{
		request, event_id
	};

	if(!check_id(event))
		throw m::BAD_REQUEST
		{
			"ID of event in request does not match path parameter %s",
			string_view{event_id},
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

	if(unquote(json::get<"content"_>(event).get("membership")) != "join")
		throw m::error
		{
			http::NOT_MODIFIED, "M_INVALID_CONTENT_MEMBERSHIP",
			"Event content.membership state must be 'join'."
		};

	if(json::get<"origin"_>(event) != request.node_id)
		throw m::error
		{
			http::NOT_MODIFIED, "M_MISMATCH_ORIGIN",
			"Event origin must be you."
		};

	if(m::room::server_acl::enable_write && !m::room::server_acl::check(room_id, request.node_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted by the room's server access control list."
		};

	// Sign the event for the latest spec; should be backwards compat.
	// Re-stringify the result so we're not relying on the thread_local.
	thread_local char sigs[4_KiB];
	const json::strung signed_event_json
	{
		signatures(sigs, event, my_host())
	};

	const m::event signed_event
	{
		signed_event_json, event_id
	};

	m::vm::opts vmopts
	{
		.node_id = request.node_id,

		// Don't conduct fetches for any references they added.
		.fetch = false,

		// Whether to conduct join amplification.
		.amplify = bool
		{
			// This is required by the spec, but...
			true

			// Only if their server has no other users joined to the room.
			&& m::room::members(room_id).empty("join", request.node_id)

			// If one of the following holds:
			&& (false

				// If we've set an alias for this room, otherwise we're not
				// an official join server and they shouldn't be using us.
				|| m::room::aliases(room_id).has_server(origin(m::my()))

				// Unless we invited them directly.
				// TODO: check if *we* invited them.
				|| m::membership(room_id, at<"state_key"_>(event), "invite")
			)
		}
	};

	m::vm::eval eval
	{
		signed_event, vmopts
	};

	m::resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	if(v1)
	{
		json::stack::array top
		{
			out
		};

		// First element is the 200
		top.append(json::value(200L));

		// Second element is the object
		json::stack::object data
		{
			top
		};

		send_join__response(client, request, signed_event, data);
		return response;
	}

	json::stack::object top
	{
		out
	};

	// Top element is the object
	send_join__response(client, request, signed_event, top);
	return response;
}

void
send_join__response(client &client,
                    const m::resource::request &request,
                    const m::event &event,
                    json::stack::object &data)
{
	const bool omit_members
	{
		request.query.get("omit_members", false)
	};

	const auto state_idx
	{
		m::index(std::nothrow, event.event_id)
	};

	const auto prev_state_idx
	{
		m::room::state::prev(state_idx)
	};

	const auto prev_state_id
	{
		m::event_id(std::nothrow, prev_state_idx)
	};

	// The room prior to this join.
	const m::room room
	{
		at<"room_id"_>(event), prev_state_id
	};

	const m::room::state state
	{
		room
	};

	const m::room::auth::chain auth_chain
	{
		prev_state_idx
	};

	// auth_chain
	if(request.query.get<bool>("auth_chain", true))
	{
		json::stack::array auth_chain_a
		{
			data, "auth_chain"
		};

		auth_chain.for_each(m::event::closure_idx_bool{[&auth_chain_a]
		(const m::event::idx &event_idx)
		{
			const m::event::fetch event
			{
				std::nothrow, event_idx
			};

			if(event.valid)
				auth_chain_a.append(event);

			return true;
		}});
	}

	// auth_chain_ids (non-spec)
	if(request.query.get<bool>("auth_chain_ids", false))
	{
		json::stack::array auth_chain_a
		{
			data, "auth_chain_ids"
		};

		auth_chain.for_each(m::event::closure_idx_bool{[&auth_chain_a]
		(const m::event::idx &event_idx)
		{
			const auto &event_id
			{
				m::event_id(std::nothrow, event_idx)
			};

			if(event_id)
				auth_chain_a.append(event_id);

			return true;
		}});
	}

	// Maybe required. Might as well...
	json::stack::member
	{
		data, "event", event.source
	};

	json::stack::member
	{
		data, "members_omitted", json::value{omit_members}
	};

	// Required. The resident server's DNS name.
	json::stack::member
	{
		data, "origin", my_host()
	};

	if(request.query.get<bool>("servers_in_room", true))
	{
		json::stack::array servers_a
		{
			data, "servers_in_room"
		};

		const m::room::origins origins
		{
			room
		};

		origins.for_each([&servers_a]
		(const string_view &origin)
		{
			servers_a.append(origin);
		});
	}

	// state
	if(request.query.get<bool>("state", true))
	{
		json::stack::array state_a
		{
			data, "state"
		};

		state.for_each([&state_a, &omit_members]
		(const m::event &event)
		{
			if(omit_members && json::get<"type"_>(event) == "m.room.member")
				return;

			state_a.append(event);
		});
	}

	// state_ids (non-spec)
	if(request.query.get<bool>("state_ids", false))
	{
		json::stack::array state_ids
		{
			data, "state_ids"
		};

		state.for_each(m::event::id::closure{[&state_ids]
		(const m::event::id &event_id)
		{
			state_ids.append(event_id);
			return true;
		}});
	}
}
