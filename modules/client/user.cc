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
get_filter(client &client,
           const resource::request &request,
           const m::user::id &user_id)
{
	m::event::id::buf filter_id
	{
		url::decode(request.parv[2], filter_id)
	};

	//TODO: ??
	const unique_buffer<mutable_buffer> buffer
	{
		m::filter::size(filter_id)
	};

	//TODO: get direct
	const m::filter filter
	{
		filter_id, buffer
	};

	return resource::response
	{
		client, json::object{buffer}
	};
}

// (5.2) Uploads a new filter definition to the homeserver. Returns a filter ID that
// may be used in future requests to restrict which events are returned to the client.
resource::response
post_filter(client &client,
            const resource::request::object<const m::filter> &request,
            const m::user::id &user_id)
{
	// (5.2) Required. The id of the user uploading the filter. The access
	// token must be authorized to make requests for this user id.
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

resource::response
put_account_data(client &client,
                 const resource::request &request,
                 const m::user::id &user_id)
{
	std::cout << "put account data: " << user_id << " " << request.content << std::endl;

	return resource::response
	{
		client, http::OK
	};
}

resource::response
get_user(client &client, const resource::request &request)
{
	if(request.parv.size() < 2)
		throw m::error
		{
			http::MULTIPLE_CHOICES, "M_NOT_FOUND", "user id required"
		};

	m::user::id::buf user_id
	{
		url::decode(request.parv[0], user_id)
	};

	const string_view &cmd
	{
		request.parv[1]
	};

	if(cmd == "filter")
		return get_filter(client, request, user_id);

	throw m::NOT_FOUND
	{
		"/user command not found"
	};
}

resource::method get_method
{
	user_resource, "GET", get_user,
	{
		get_method.REQUIRES_AUTH
	}
};

resource::response
post_user(client &client, resource::request &request)
{
	if(request.parv.size() < 2)
		throw m::error
		{
			http::MULTIPLE_CHOICES, "M_NOT_FOUND", "user id required"
		};

	m::user::id::buf user_id
	{
		url::decode(request.parv[0], user_id)
	};

	const string_view &cmd
	{
		request.parv[1]
	};

	if(cmd == "filter")
		return post_filter(client, request, user_id);

	throw m::NOT_FOUND
	{
		"/user command not found"
	};
}

resource::method post_method
{
	user_resource, "POST", post_user,
	{
		post_method.REQUIRES_AUTH
	}
};

resource::response
put_user(client &client, const resource::request &request)
{
	if(request.parv.size() < 2)
		throw m::error
		{
			http::MULTIPLE_CHOICES, "M_NOT_FOUND", "user id required"
		};

	m::user::id::buf user_id
	{
		url::decode(request.parv[0], user_id)
	};

	const string_view &cmd
	{
		request.parv[1]
	};

	if(cmd == "account_data")
		return put_account_data(client, request, user_id);

	throw m::NOT_FOUND
	{
		"/user command not found"
	};
}

resource::method put_method
{
	user_resource, "PUT", put_user,
	{
		put_method.REQUIRES_AUTH
	}
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/user' to handle requests"
};
