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

// federation_invite (weak)
extern conf::item<milliseconds>
stream_cross_sleeptime;

static void
check_event(const m::resource::request &request,
            const m::event &event);

static void
process(client &,
        const m::resource::request &,
        const m::event &,
        const string_view &room_version);

static m::resource::response
put__invite2(client &client,
             const m::resource::request &request);

static m::resource::response
put__invite(client &client,
            const m::resource::request &request);

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

m::resource
invite_resource
{
	"/_matrix/federation/v1/invite/",
	{
		invite_description,
		resource::DIRECTORY
	}
};

m::resource::method
method_put
{
	invite_resource, "PUT", put__invite,
	{
		method_put.VERIFY_ORIGIN
	}
};

conf::item<milliseconds>
IRCD_MODULE_EXPORT_DATA
stream_cross_sleeptime
{
	{ "name",    "ircd.federation.invite.stream_cross_sleeptime" },
	{ "default", 5000L                                           },
};

m::resource::response
put__invite(client &client,
            const m::resource::request &request)
{
	if(request.version != "v1")
		return put__invite2(client, request);

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

	if(m::room::server_acl::enable_write && !m::room::server_acl::check(room_id, request.node_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted by the room's server access control list."
		};

	check_event(request, event);
	const m::user::id target
	{
		at<"state_key"_>(event)
	};

	if(m::membership(room_id, target, m::membership_positive))
		throw m::error
		{
			http::NOT_MODIFIED, "M_MEMBER_EXISTS",
			"User is already joined or invited to this room.",
		};

	thread_local char sigs[4_KiB];
	const m::event signed_event
	{
		signatures(sigs, event, target.host())
	};

	const json::strung revent
	{
		signed_event
	};

	const json::value array[2]
	{
		json::value{200L}, json::members
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
	m::resource::response response
	{
		client, json::value
		{
			array, 2
		}
	};

	// Synapse needs time to process our response otherwise our eval below may
	// complete before this response arrives for them and is processed.
	ctx::sleep(milliseconds(stream_cross_sleeptime));

	// Process the event
	process(client, request, signed_event, room_version);

	// note: returning a resource response is a symbolic/indicator action to
	// the caller and has no real effect at the point of return.
	return response;
}

void
check_event(const m::resource::request &request,
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

	if(at<"origin"_>(event) != request.node_id)
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
	const m::event::conforms report
	{
		event, non_conforms.report
	};

	if(!report.clean())
		throw m::error
		{
			http::NOT_MODIFIED, "M_INVALID_EVENT",
			"Proffered event has the following problems :%s",
			string(report)
		};

	if(!verify(event, request.node_id))
		throw m::ACCESS_DENIED
		{
			"Invite event fails verification for %s", request.node_id
		};
}

m::resource::response
put__invite2(client &client,
             const m::resource::request &request)
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

	m::event event
	{
		request["event"], event_id
	};

	if(!json::get<"event_id"_>(event))
		if(room_version == "1" || room_version == "2")
			json::get<"event_id"_>(event) = event_id;

	if(!check_id(event, room_version))
		throw m::BAD_REQUEST
		{
			"Claimed event_id %s is incorrect.",
			string_view{event_id},
		};

	if(at<"room_id"_>(event) != room_id)
		throw m::error
		{
			http::NOT_MODIFIED, "M_MISMATCH_ROOM_ID",
			"ID of room in request body %s does not match path param %s",
			string_view{at<"room_id"_>(event)},
			string_view{room_id},
		};

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

	if(at<"origin"_>(event) != request.node_id)
		throw m::error
		{
			http::FORBIDDEN, "M_INVALID_ORIGIN",
			"event.origin must be you."
		};

	const m::user::id &sender
	{
		at<"sender"_>(event)
	};

	if(sender.host() != request.node_id)
		throw m::error
		{
			http::FORBIDDEN, "M_INVALID_ORIGIN",
			"event.sender must be your user."
		};

	const m::user::id &target
	{
		at<"state_key"_>(event)
	};

	if(!my_host(target.host()))
		throw m::error
		{
			http::FORBIDDEN, "M_INVALID_STATE_KEY",
			"event.state_key must be my user."
		};

	if(m::membership(room_id, target, m::membership_positive))
		throw m::error
		{
			http::NOT_MODIFIED, "M_MEMBER_EXISTS",
			"User is already joined or invited to this room.",
		};

	m::event::conforms non_conforms;
	const m::event::conforms report
	{
		event, non_conforms.report
	};

	if(!report.clean())
		throw m::error
		{
			http::NOT_MODIFIED, "M_INVALID_EVENT",
			"Proffered event has the following problems :%s",
			string(report)
		};

	// May conduct disk IO to check ACL
	if(m::room::server_acl::enable_write)
		if(!m::room::server_acl::check(room_id, request.node_id))
			throw m::ACCESS_DENIED
			{
				"You are not permitted by the room's server access control list."
			};

	// May conduct network IO to fetch node's key; disk IO to fetch node's key
	if(!verify(event, request.node_id))
		throw m::ACCESS_DENIED
		{
			"Invite event fails verification for %s",
			string_view{request.node_id},
		};

	thread_local char sigs[4_KiB];
	m::event signed_event
	{
		signatures(sigs, event, target.host())
	};

	signed_event.event_id = event_id;
	const json::strung signed_json
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
	m::resource::response response
	{
		client, json::members
		{
			{ "event", json::object{signed_json} }
		}
	};

	// Synapse needs time to process our response otherwise our eval below may
	// complete before this response arrives for them and is processed.
	ctx::sleep(milliseconds(stream_cross_sleeptime));

	// Post processing, does not throw.
	process(client, request, signed_event, room_version);

	// note: returning a resource response is a symbolic/indicator action to
	// the caller and has no real effect at the point of return.
	return response;
}

void
process(client &client,
        const m::resource::request &request,
        const m::event &event,
        const string_view &room_version)
try
{
	// Eval the dual-signed invite event. This will write it locally. This will
	// also try to sync the room as best as possible. The invitee will then be
	// presented with this invite request in their rooms list.
	m::vm::opts vmopts;
	vmopts.node_id = request.node_id;

	// Synapse may 403 a fetch of the prev_event of the invite event.
	vmopts.phase.set(m::vm::phase::FETCH_PREV, false);

	// Synapse now 403's a fetch of auth_events of the invite event
	vmopts.auth = false;

	// Don't throw an exception for a re-evaluation; this will just be a no-op
	vmopts.nothrows |= m::vm::fault::EXISTS;
	vmopts.room_version = room_version;

	// Find any "unsigned" field robust to any version of the specapse
	const json::object _unsigned
	{
		request["unsigned"]?:
			json::object(request["event"]).get("unsigned")
	};

	// Find any "invite_room_state" robust to any version of the synspec
	const json::array &invite_room_state
	{
		request["invite_room_state"]?:
			_unsigned["invite_room_state"]
	};

	// The remote maybe gave us some events they purport to be the room's
	// state. If so, we can try to evaluate them to give our client more info.
	if((false) && !empty(invite_room_state))
	{
		// Chances are these are incomplete/malformed events which will fail
		// conformity checks and not be used, so this will likely do nothing.
		auto irs_vmopts(vmopts);
		irs_vmopts.nothrows = -1;
		irs_vmopts.phase.set(m::vm::phase::EMPTION, false);
		m::vm::eval
		{
			invite_room_state, irs_vmopts
		};
	}

	m::vm::eval
	{
		event, vmopts
	};
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "Processing invite from:%s to:%s :%s",
		json::get<"sender"_>(event),
		json::get<"state_key"_>(event),
		e.what(),
	};

	return;
}
