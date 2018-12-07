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
	"Federation :Send leave event"
};

const string_view
send_leave_description
{R"(

Inject a leave event into a room originating from a server without any joined
users in that room.

)"};

resource
send_leave_resource
{
	"/_matrix/federation/v1/send_leave/",
	{
		send_leave_description,
		resource::DIRECTORY
	}
};

resource::response
put__send_leave(client &client,
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

	if(!my_host(room_id.host()))
		throw m::error
		{
			http::FORBIDDEN, "M_INVALID_ROOM_ID",
			"Can only send_leave for rooms on my host '%s'",
			my_host()
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

	if(json::get<"membership"_>(event) && json::get<"membership"_>(event) != "leave")
		throw m::error
		{
			http::NOT_MODIFIED, "M_INVALID_MEMBERSHIP",
			"Event membership state must be 'leave'."
		};

	if(unquote(json::get<"content"_>(event).get("membership")) != "leave")
		throw m::error
		{
			http::NOT_MODIFIED, "M_INVALID_CONTENT_MEMBERSHIP",
			"Event content.membership state must be 'leave'."
		};

	if(json::get<"origin"_>(event) != request.origin)
		throw m::error
		{
			http::NOT_MODIFIED, "M_MISMATCH_ORIGIN",
			"Event origin must be you."
		};

	m::vm::opts vmopts;
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	vmopts.non_conform.set(m::event::conforms::MISSING_MEMBERSHIP);
	m::vm::eval eval
	{
		event, vmopts
	};

	return resource::response
	{
		client, http::OK
	};
}

resource::method
method_put
{
	send_leave_resource, "PUT", put__send_leave,
	{
		method_put.VERIFY_ORIGIN
	}
};
