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

resource user_resource
{
	"/_matrix/client/r0/user/",
	{
		"User resource",
		resource::DIRECTORY,
	}
};

resource::response
get_filter(client &client, const resource::request &request)
{
	m::user::id::buf user_id
	{
		url::decode(request.parv[0], user_id)
	};

	const auto &filter_id
	{
		request.parv[2]
	};

	const m::vm::query<m::vm::where::equal> query
	{
		{ "room_id",      m::filter::filters.room_id  },
		{ "type",        "ircd.filter"                },
		{ "state_key",    filter_id                   },
		{ "sender",       user_id                     },
	};

	const auto result{[&client]
	(const m::event &event)
	{
		const json::object &filter
		{
			json::at<"content"_>(event)
		};

		resource::response
		{
			client, filter
		};

		return true;
	}};

	if(!m::vm::test(query, result))
		throw m::NOT_FOUND("No matching filter with that ID");

	// Response already made
	return {};
}

resource::method get_method
{
	user_resource, "GET", get_filter,
	{
		get_method.REQUIRES_AUTH
	}
};

// (5.2) Uploads a new filter definition to the homeserver. Returns a filter ID that
// may be used in future requests to restrict which events are returned to the client.
resource::response
post_filter(client &client, const resource::request::object<const m::filter> &request)
{
	// (5.2) Required. The id of the user uploading the filter. The access
	// token must be authorized to make requests for this user id.
	m::user::id::buf user_id
	{
		url::decode(request.parv[0], user_id)
	};

	if(user_id != request.user_id)
		throw m::ACCESS_DENIED
		{
			"Trying to post a filter for `%s' but you are `%s'",
			user_id,
			request.user_id
		};

	// (5.2) List of event fields to include. If this list is absent then all fields are
	// included. The entries may include '.' charaters to indicate sub-fields. So
	// ['content.body'] will include the 'body' field of the 'content' object. A literal '.'
	// character in a field name may be escaped using a '\'. A server may include more
	// fields than were requested.
	const auto &event_fields
	{
		json::get<"event_fields"_>(request)
	};

	// (5.2) The format to use for events. 'client' will return the events in a format suitable
	// for clients. 'federation' will return the raw event as receieved over federation.
	// The default is 'client'. One of: ["client", "federation"]
	const auto &event_format
	{
		json::get<"event_format"_>(request)
	};

	// (5.2) The user account data that isn't associated with rooms to include.
	const auto &account_data
	{
		json::get<"account_data"_>(request)
	};

	// (5.2) Filters to be applied to room data.
	const auto &room
	{
		json::get<"room"_>(request)
	};

	const auto &state
	{
		json::get<"state"_>(room)
	};

	const auto &presence
	{
		// (5.2) The presence updates to include.
		json::get<"presence"_>(request)
	};

	const auto filter_id
	{
		send(m::filter::filters, user_id, "ircd.filter"_sv, request.body)
	};

	return resource::response
	{
		client, http::CREATED,
		{
			{ "filter_id", filter_id }
		}
	};
}

resource::method post_method
{
	user_resource, "POST", post_filter,
	{
		post_method.REQUIRES_AUTH
	}
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/user' to handle requests"
};
