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

static void _rejoin_room(const m::room &room, const m::user &user);
static void _rejoin_rooms(const m::user::id &user_id);
static void handle_my_profile_changed__displayname(const m::event &event);
static void handle_my_profile_changed__avatar_url(const m::event &event);
static void handle_my_profile_changed(const m::event &, m::vm::eval &);
static resource::response get__profile_full(client &, const resource::request &, const m::user &);
static resource::response get__profile_remote(client &, const resource::request &, const m::user &);
static resource::response get__profile(client &, const resource::request &);
static resource::response put__profile(client &, const resource::request &);

mapi::header
IRCD_MODULE
{
	"Client 8.2 :Profiles"
};

const size_t
PARAM_MAX_SIZE
{
	128
};

const m::hookfn<m::vm::eval &>
my_profile_changed
{
	handle_my_profile_changed,
	{
		{ "_site",  "vm.effect"     },
		{ "type",   "ircd.profile"  },
	}
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

resource::method
method_get
{
	profile_resource, "GET", get__profile
};

resource::method
method_put
{
	profile_resource, "PUT", put__profile,
	{
		method_put.REQUIRES_AUTH
	}
};

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

	char parambuf[PARAM_MAX_SIZE];
	const string_view &param
	{
		url::decode(parambuf, request.parv[1])
	};

	const string_view &value
	{
		unquote(request.at(param))
	};

	const m::user::profile profile
	{
		user
	};

	bool modified{true};
	profile.get(std::nothrow, param, [&value, &modified]
	(const string_view &param, const string_view &existing)
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
		profile.set(param, value)
	};

	return resource::response
	{
		client, http::OK
	};
}

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

	char parambuf[PARAM_MAX_SIZE];
	const string_view &param
	{
		url::decode(parambuf, request.parv[1])
	};

	const m::user::profile profile
	{
		user
	};

	profile.get(param, [&client]
	(const string_view &param, const string_view &value)
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

resource::response
get__profile_full(client &client,
                  const resource::request &request,
                  const m::user &user)
{
	const m::user::profile profile
	{
		user
	};

	// Have to return a 404 if the profile is empty rather than a {},
	// so we iterate for at least one element first to check that.
	bool empty{true};
	profile.for_each([&empty]
	(const string_view &, const string_view &)
	{
		empty = false;
		return false;
	});

	if(empty)
		throw m::NOT_FOUND
		{
			"Profile for %s is empty.",
			string_view{user.user_id}
		};

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::object top
	{
		out
	};

	profile.for_each([&top]
	(const string_view &param, const string_view &value)
	{
		json::stack::member
		{
			top, param, value
		};

		return true;
	});

	return {};
}

conf::item<seconds>
remote_request_timeout
{
	{ "name",    "ircd.client.profile.remote_request.timeout" },
	{ "default", 10L                                          }
};

resource::response
get__profile_remote(client &client,
                    const resource::request &request,
                    const m::user &user)
try
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
		user.user_id, param, buf, m::v1::query::opts
		{
			user.user_id.host()
		}
	};

	const seconds &timeout(remote_request_timeout);
	if(!federation_request.wait(timeout, std::nothrow))
		throw m::error
		{
			http::GATEWAY_TIMEOUT, "M_PROFILE_TIMEOUT",
			"Server '%s' did not respond with profile for %s in time.",
			user.user_id.host(),
			string_view{user.user_id}
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
catch(const http::error &)
{
	throw;
}
catch(const server::unavailable &e)
{
	throw m::error
	{
		http::SERVICE_UNAVAILABLE, "M_PROFILE_UNAVAILABLE",
		"Server '%s' cannot be contacted for profile of %s :%s",
		user.user_id.host(),
		string_view{user.user_id},
		e.what()
	};
}
catch(const server::error &e)
{
	throw m::error
	{
		http::BAD_GATEWAY, "M_PROFILE_UNAVAILABLE",
		"Error when contacting '%s' for profile of %s :%s",
		user.user_id.host(),
		string_view{user.user_id},
		e.what()
	};
}

m::event::id::buf
IRCD_MODULE_EXPORT
ircd::m::user::profile::set(const m::user &user,
                            const string_view &key,
                            const string_view &val)
{
	const m::user::room user_room
	{
		user
	};

	return m::send(user_room, user, "ircd.profile", key,
	{
		{ "text", val }
	});
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::profile::get(std::nothrow_t,
                            const m::user &user,
                            const string_view &key,
                            const closure &closure)
{
	const user::room user_room{user};
	const room::state state{user_room};
	const event::idx event_idx
	{
		state.get(std::nothrow, "ircd.profile", key)
	};

	return event_idx && m::get(std::nothrow, event_idx, "content", [&closure, &key]
	(const json::object &content)
	{
		const string_view &value
		{
			unquote(content.get("text"))
		};

		closure(key, value);
	});
}

bool
IRCD_MODULE_EXPORT
ircd::m::user::profile::for_each(const m::user &user,
                                 const closure_bool &closure)
{
	static const event::fetch::opts fopts
	{
		event::keys::include {"content", "state_key"}
	};

	const user::room user_room{user};
	const room::state state
	{
		user_room, &fopts
	};

	return state.for_each("ircd.profile", event::closure_bool{[&closure]
	(const event &event)
	{
		const string_view &key
		{
			at<"state_key"_>(event)
		};

		const string_view &value
		{
			unquote(json::get<"content"_>(event).get("text"))
		};

		return closure(key, value);
	}});
}

void
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

void
handle_my_profile_changed__avatar_url(const m::event &event)
{
	_rejoin_rooms(at<"sender"_>(event));
}

void
handle_my_profile_changed__displayname(const m::event &event)
{
	_rejoin_rooms(at<"sender"_>(event));
}

void
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

void
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
