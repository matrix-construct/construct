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
check_event(const resource::request &request,
            const m::event &event);

static resource::response
put__invite(client &client,
            const resource::request &request);

mapi::header
IRCD_MODULE
{
	"Federation 10 :Inviting to a room"
};

const string_view
invite_description
{R"(
When a user wishes to invite an other user to a local room and the other
user is on a different server, the inviting server will send a request to
the invited server.
)"};

resource
invite_resource
{
	"/_matrix/federation/v1/invite/",
	{
		invite_description,
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

	const m::event event
	{
		request
	};

	if(at<"event_id"_>(event) != event_id)
		throw m::error
		{
			http::NOT_MODIFIED, "M_MISMATCH_EVENT_ID",
			"ID of event in request body does not match the path parameter."
		};

	if(at<"room_id"_>(event) != room_id)
		throw m::error
		{
			http::NOT_MODIFIED, "M_MISMATCH_ROOM_ID",
			"ID of room in request body does not match the path parameter."
		};

	check_event(request, event);

	thread_local char sigs[4_KiB];
	const m::event signed_event
	{
		signatures(sigs, event)
	};

	const json::strung revent
	{
		signed_event
	};

	const json::value array[2]
	{
		json::value
		{
			200L
		},
		json::members
		{
			{ "event", revent }
		}
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
		client, json::value
		{
			array, 2
		}
	};

	// Eval the dual-signed invite event. This will write it locally. This will
	// also try to sync the room as best as possible. The invitee will then be
	// presented with this invite request in their rooms list.
	m::vm::opts vmopts;
	vmopts.prev_check_exists = false;

	// We don't want this eval throwing an exception because the response has
	// already been made for this request.
	const unwind::nominal::assertion na;
	vmopts.nothrows = -1;

	m::vm::eval
	{
		signed_event, vmopts
	};

	// The remote maybe gave us some events they purport to be the room's
	// state. If so, we can try to evaluate them to give our client more info.
	const json::array &invite_room_state
	{
		json::object(request["unsigned"]).get("invite_room_state")
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
}
