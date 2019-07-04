// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

static void
check_event(const resource::request &request,
            const m::event &event);

static resource::response
put__invite(client &client,
            const resource::request &request);

mapi::header
IRCD_MODULE
{
	"Federation 12 :Inviting to a room (v2)"
};

resource
invite_resource
{
	"/_matrix/federation/v2/invite/",
	{
		"Inviting to a room",
		resource::DIRECTORY
	}
};

resource::method
method_put
{
	invite_resource, "PUT", put__invite,
	{
		method_put.VERIFY_ORIGIN
	}
};

mods::import<conf::item<milliseconds>>
stream_cross_sleeptime
{
	"federation_invite", "stream_cross_sleeptime"
};

resource::response
put__invite(client &client,
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
			"event_id path parameter required"
		};

	m::event::id::buf event_id
	{
		url::decode(event_id, request.parv[1])
	};

	const json::string &room_version
	{
		request.get("room_version", "1")
	};

	const json::array &invite_room_state
	{
		request["invite_room_version"]
	};

	m::event event
	{
		request["event"]
	};

	char check_buf[48];
	m::event::id check_id; switch(hash(room_version))
	{
		case "1"_:
		case "2"_:
			check_id = at<"event_id"_>(event);
			break;

		case "3"_:
			check_id = m::event::id::v3{check_buf, event};
			break;

		case "4"_:
		case "5"_:
		default:
			check_id = m::event::id::v4{check_buf, event};
			break;
	}

	if(!check_id || event_id != check_id)
		throw m::BAD_REQUEST
		{
			"Claimed event_id %s does not match %s",
			string_view{event_id},
			string_view{check_id},
		};

	if(at<"room_id"_>(event) != room_id)
		throw m::error
		{
			http::NOT_MODIFIED, "M_MISMATCH_ROOM_ID",
			"ID of room in request body %s does not match path param %s",
			string_view{at<"room_id"_>(event)},
			string_view{room_id},
		};

	if(m::room::server_acl::enable_write)
		if(!m::room::server_acl::check(room_id, request.node_id))
			throw m::ACCESS_DENIED
			{
				"You are not permitted by the room's server access control list."
			};

	check_event(request, event);

	thread_local char sigs[4_KiB];
	m::event signed_event
	{
		signatures(sigs, event)
	};

	const json::strung revent
	{
		signed_event
	};

	// Send back the signed event first before eval. If we eval the signed
	// event first: the effects will occur before the inviting server has
	// the signed event returned from us; they might not consider the user
	// invited yet, causing trouble for the eval effects. That may actually
	// still happen due to the two separate TCP connections being uncoordinated
	// (one for this request, and another when m::eval effects connect to them
	// and make any requests). But either way if this call fails then we will
	// lose the invite but that may not be such a bad thing.
	resource::response response
	{
		client, json::object{revent}
	};

	// Synapse needs time to process our response otherwise our eval below may
	// complete before this response arrives for them and is processed.
	ctx::sleep(milliseconds(*stream_cross_sleeptime));

	// Eval the dual-signed invite event. This will write it locally. This will
	// also try to sync the room as best as possible. The invitee will then be
	// presented with this invite request in their rooms list.
	m::vm::opts vmopts;
	vmopts.node_id = request.origin;

	// Synapse may 403 a fetch of the prev_event of the invite event.
	vmopts.fetch_prev_check = false;
	vmopts.fetch_prev = false;

	// We don't want this eval throwing an exception because the response has
	// already been made for this request.
	const unwind::nominal::assertion na;
	vmopts.nothrows = -1;

	m::vm::eval
	{
		signed_event, vmopts
	};

	if(!empty(invite_room_state))
	{
		m::vm::eval
		{
			invite_room_state, vmopts
		};
	};

	// note: returning a resource response is a symbolic/indicator action to
	// the caller and has no real effect at the point of return.
	return response;
}

void
check_event(const resource::request &request,
            const m::event &event)
{
	if(at<"type"_>(event) != "m.room.member")
		throw m::error
		{
			http::NOT_MODIFIED, "M_INVALID_TYPE",
			"event.type must be m.room.member"
		};

	if(unquote(at<"content"_>(event).at("membership")) != "invite")
		throw m::error
		{
			http::NOT_MODIFIED, "M_INVALID_CONTENT_MEMBERSHIP",
			"event.content.membership must be invite."
		};

	if(at<"origin"_>(event) != request.origin)
		throw m::error
		{
			http::FORBIDDEN, "M_INVALID_ORIGIN",
			"event.origin must be you."
		};

	if(!my_host(m::user::id(at<"state_key"_>(event)).host()))
		throw m::error
		{
			http::FORBIDDEN, "M_INVALID_STATE_KEY",
			"event.state_key must be my user."
		};

	m::event::conforms non_conforms;
	non_conforms |= non_conforms.MISSING_PREV_STATE;
	non_conforms |= non_conforms.INVALID_OR_MISSING_EVENT_ID;
	const m::event::conforms report
	{
		event, non_conforms.report
	};

	if(!report.clean())
		throw m::error
		{
			http::NOT_MODIFIED, "M_INVALID_EVENT",
			"Proffered event has the following problems: %s",
			string(report)
		};

	if(!verify(event, request.node_id))
		throw m::ACCESS_DENIED
		{
			"Invite event fails verification for %s", request.node_id
		};
}
