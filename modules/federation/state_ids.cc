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
		url::decode(request.query.at("event_id"), event_id);

	 //TODO: direct to socket
	const unique_buffer<mutable_buffer> buf{1_MiB}; //TODO: XXX
	json::stack out{buf};
	{
		json::stack::object top{out};
		json::stack::member pdus_m
		{
			top, "pdu_ids"
		};

		json::stack::array pdus
		{
			pdus_m
		};

		const m::room::state state
		{
			m::room
			{
				room_id, event_id
			}
		};

		state.for_each([&pdus]
		(const m::event &event)
		{
			const auto event_id
			{
				at<"event_id"_>(event)
			};

			pdus.append(event_id);
		});
	}

	return resource::response
	{
		client, json::object{out.completed()}
	};
}

resource::method
method_get
{
	state_ids_resource, "GET", get__state_ids,
	{
		method_get.VERIFY_ORIGIN
	}
};
