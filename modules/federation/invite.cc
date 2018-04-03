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

static void
check_event(const resource::request &request,
            const m::event &event);

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
		url::decode(request.parv[0], room_id)
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

	check_event(request, event);

	//TODO: eval()

	const json::strung event_strung
	{
		event
	};

	const json::member revent
	{
		"event", event_strung
	};

	const json::value response[]
	{
		json::value { 200L },
		json::value { &revent, 1 },
	};

	return resource::response
	{
		client, json::value
		{
			response, 2
		}
	};
}

resource::method
method_put
{
	invite_resource, "PUT", put__invite,
	{
		method_put.VERIFY_ORIGIN
	}
};

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

	if(at<"membership"_>(event) != "invite")
		throw m::error
		{
			http::NOT_MODIFIED, "M_INVALID_MEMBERSHIP",
			"event.membership must be invite."
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
