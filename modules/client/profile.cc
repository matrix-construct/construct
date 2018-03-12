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
	"Client 8.2 :Profiles"
};

ircd::resource
profile_resource
{
	"/_matrix/client/r0/profile/",
	{
		"(8.2) Profiles",
		resource::DIRECTORY,
	}
};

static resource::response
get__profile_full(client &client,
                  const resource::request &request,
                  const m::room &user_room);

static resource::response
get__profile_remote(client &client,
                    const resource::request &request,
                    const m::user &user);

resource::response
get__profile(client &client,
             const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"user_id path parameter required"
		};

	m::user::id::buf user_id
	{
		url::decode(request.parv[0], user_id)
	};

	const m::user user
	{
		user_id
	};

	const m::room::id::buf user_room_id
	{
		user.room_id()
	};

	if(!my(user) && !exists(user_room_id))
		return get__profile_remote(client, request, user);

	const m::room user_room
	{
		user_room_id
	};

	if(request.parv.size() < 2)
		return get__profile_full(client, request, user_room);

	const string_view &param
	{
		request.parv[1]
	};

	user_room.get("ircd.profile", param, [&client, &param]
	(const m::event &event)
	{
		const string_view &value
		{
			at<"content"_>(event).at("text")
		};

		resource::response
		{
			client, json::members
			{
				{ param, value }
			}
		};
	});

	return {}; // responded from closure
}

resource::method
method_get
{
	profile_resource, "GET", get__profile
};

resource::response
get__profile_full(client &client,
                  const resource::request &request,
                  const m::room &user_room)
{
	const m::room::state state
	{
		user_room
	};

	std::vector<json::member> members;
	state.for_each("ircd.profile", [&members]
	(const m::event &event)
	{
		const auto &key
		{
			at<"state_key"_>(event)
		};

		const auto &value
		{
			at<"content"_>(event).at("text")
		};

		members.emplace_back(key, value);
	});

	const json::strung response
	{
		data(members), data(members) + size(members)
	};

	return resource::response
	{
		client, json::object
		{
			response
		}
	};
}

resource::response
get__profile_remote(client &client,
                    const resource::request &request,
                    const m::user &user)
{
	//TODO: XXX cache strat user's room

	const string_view &field
	{
		request.parv[1]
	};

	const unique_buffer<mutable_buffer> buf
	{
		64_KiB
	};

	m::v1::query::profile federation_request
	{
		user.user_id, field, buf
	};

	//TODO: conf
	if(federation_request.wait(seconds(8)) == ctx::future_status::timeout)
		throw http::error
		{
			http::REQUEST_TIMEOUT
		};

	const http::code &code
	{
		federation_request.get()
	};

	const json::object &response
	{
		federation_request
	};

	//TODO: cache into a room with ttl

	return resource::response
	{
		client, response
	};
}

resource::response
put__profile(client &client,
              const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"user_id path parameter required"
		};

	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"profile property path parameter required"
		};

	m::user::id::buf user_id
	{
		url::decode(request.parv[0], user_id)
	};

	if(user_id != request.user_id)
		throw m::FORBIDDEN
		{
			"Trying to set profile for '%s' but you are '%s'",
			user_id,
			request.user_id
		};

	const m::user user
	{
		user_id
	};

	const m::room::id::buf user_room_id
	{
		user.room_id()
	};

	const m::room user_room
	{
		user_room_id
	};

	const string_view &param
	{
		request.parv[1]
	};

	const string_view &value
	{
		unquote(request.at(param))
	};

	send(user_room, request.user_id, "ircd.profile", param,
	{
		{ "text", value }
	});

	return resource::response
	{
		client, http::OK
	};
}

resource::method
method_put
{
	profile_resource, "PUT", put__profile,
	{
		method_put.REQUIRES_AUTH
	}
};
