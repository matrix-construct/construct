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

extern "C" void
profile_get(const m::user &user,
            const string_view &key,
            const m::user::profile_closure &closure);

extern "C" m::event::id::buf
profile_set(const m::user &user,
            const m::user &sender,
            const string_view &key,
            const string_view &value);

static resource::response
get__profile_full(client &,
                  const resource::request &,
                  const m::user &);

static resource::response
get__profile_remote(client &,
                    const resource::request &,
                    const m::user &);

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
		url::decode(user_id, request.parv[0])
	};

	const m::user user
	{
		user_id
	};

	if(!my(user) && !exists(user))
		return get__profile_remote(client, request, user);

	if(request.parv.size() < 2)
		return get__profile_full(client, request, user);

	char parambuf[128];
	const string_view &param
	{
		url::decode(parambuf, request.parv[1])
	};

	profile_get(user, param, [&param, &client]
	(const string_view &value)
	{
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
                  const m::user &user)
{
	const m::user::room user_room{user};
	const m::room::state state{user_room};

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

	char parambuf[128];
	const string_view &param
	{
		url::decode(parambuf, request.parv[1])
	};

	const unique_buffer<mutable_buffer> buf
	{
		64_KiB
	};

	m::v1::query::profile federation_request
	{
		user.user_id, param, buf
	};

	//TODO: conf
	if(!federation_request.wait(seconds(8), std::nothrow))
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
		url::decode(user_id, request.parv[0])
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

	char parambuf[128];
	const string_view &param
	{
		url::decode(parambuf, request.parv[1])
	};

	const string_view &value
	{
		unquote(request.at(param))
	};

	const m::user::id &sender
	{
		request.user_id
	};

	bool modified{true};
	user.profile(std::nothrow, param, [&value, &modified]
	(const string_view &existing)
	{
		modified = existing != value;
	});

	if(!modified)
		return resource::response
		{
			client, http::OK
		};

	const auto eid
	{
		profile_set(user, sender, param, value)
	};

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
		method_put.REQUIRES_AUTH,
		60s
	}
};

void
profile_get(const m::user &user,
            const string_view &key,
            const m::user::profile_closure &closure)
try
{
	const m::user::room user_room
	{
		user
	};

	user_room.get("ircd.profile", key, [&closure]
	(const m::event &event)
	{
		const string_view &value
		{
			unquote(at<"content"_>(event).at("text"))
		};

		closure(value);
	});
}
catch(const m::NOT_FOUND &e)
{
	throw m::NOT_FOUND
	{
		"Nothing about '%s' and/or profile for '%s'",
		key,
		string_view{user.user_id}
	};
}

m::event::id::buf
profile_set(const m::user &user,
            const m::user &sender,
            const string_view &key,
            const string_view &value)
{
	const m::user::room user_room
	{
		user
	};

	return send(user_room, sender, "ircd.profile", key,
	{
		{ "text", value }
	});
}

static void
_rejoin_room(const m::room &room,
             const m::user &user)
try
{
	m::join(room, user);
}
catch(const std::exception &e)
{
	log::error
	{
		"Failed to rejoin '%s' to room '%s' to update profile",
		string_view{user.user_id},
		string_view{room.room_id}
	};
}

static void
_rejoin_rooms(const m::user::id &user_id)
{
	assert(my(user_id));
	const m::user::rooms &rooms
	{
		user_id
	};

	rooms.for_each("join", [&user_id]
	(const m::room &room, const string_view &)
	{
		_rejoin_room(room, user_id);
	});
}

static void
handle_my_profile_changed__displayname(const m::event &event)
{
	_rejoin_rooms(at<"sender"_>(event));
}

static void
handle_my_profile_changed__avatar_url(const m::event &event)
{
	_rejoin_rooms(at<"sender"_>(event));
}

static void
handle_my_profile_changed(const m::event &event,
                          m::vm::eval &eval)
{
	if(!my(event))
		return;

	const m::user::id &user_id
	{
		json::get<"sender"_>(event)
	};

	// The event has to be an ircd.profile in the user's room, not just a
	// random ircd.profile typed event in some other room...
	const m::user::room user_room{user_id};
	if(json::get<"room_id"_>(event) != user_room.room_id)
		return;

	if(json::get<"state_key"_>(event) == "displayname")
		return handle_my_profile_changed__displayname(event);

	if(json::get<"state_key"_>(event) == "avatar_url")
		return handle_my_profile_changed__avatar_url(event);
}

const m::hookfn<m::vm::eval &>
my_profile_changed
{
	handle_my_profile_changed,
	{
		{ "_site",  "vm.effect"     },
		{ "type",   "ircd.profile"  },
	}
};
