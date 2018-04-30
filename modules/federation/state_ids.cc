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
	"federation state_ids"
};

resource
state_ids_resource
{
	"/_matrix/federation/v1/state_ids/",
	{
		"federation state_ids",
		resource::DIRECTORY,
	}
};

resource::response
get__state_ids(client &client,
               const resource::request &request)
{
	m::room::id::buf room_id
	{
		url::decode(request.parv[0], room_id)
	};

	m::event::id::buf event_id;
	if(request.query["event_id"])
		event_id = url::decode(request.query.at("event_id"), event_id);

	const m::room room
	{
		room_id, event_id
	};

	const m::room::state state
	{
		room
	};

	const unique_buffer<mutable_buffer> buf
	{
		8_KiB
	};

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out{buf, [&response]
	(const const_buffer &buf)
	{
		response.write(buf);
		return buf;
	}};

	json::stack::object top{out};
	json::stack::member pdus_m
	{
		top, "pdu_ids"
	};

	json::stack::array pdus
	{
		pdus_m
	};

	state.for_each(m::event::id::closure{[&pdus]
	(const m::event::id &event_id)
	{
		pdus.append(event_id);
	}});

	return {};
}

resource::method
method_get
{
	state_ids_resource, "GET", get__state_ids,
	{
		method_get.VERIFY_ORIGIN
	}
};
