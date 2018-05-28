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
	"Client 11.6 :Presence"
};

ircd::resource
presence_resource
{
	"/_matrix/client/r0/presence/", resource::opts
	{
		"(11.6.2) Presence",
		resource::DIRECTORY,
	}
};

//
// get
//

static resource::response
get__presence(client &,
              const resource::request &);

resource::method
method_get
{
	presence_resource, "GET", get__presence
};

static resource::response
get__presence_status(client &,
                     const resource::request &,
                     const m::user::id &);

static resource::response
get__presence_list(client &,
                   const resource::request &);

resource::response
get__presence(client &client,
              const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"user_id or command required"
		};

	if(request.parv[0] == "list")
		return get__presence_list(client, request);

	m::user::id::buf user_id
	{
		url::decode(request.parv[0], user_id)
	};

	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"command required"
		};

	const auto &cmd
	{
		request.parv[1]
	};

	if(cmd == "status")
		return get__presence_status(client, request, user_id);

	throw m::NOT_FOUND
	{
		"Presence command not found"
	};
}

resource::response
get__presence_status(client &client,
                     const resource::request &request,
                     const m::user::id &user_id)
{
	const m::user user
	{
		user_id
	};

	m::presence::get(user, [&client]
	(const json::object &object)
	{
		resource::response
		{
			client, object
		};
	});

	return {}; // responded from closure or threw
}

resource::response
get__presence_list(client &client,
                   const resource::request &request)
{
	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"user_id required"
		};

	m::user::id::buf user_id
	{
		url::decode(request.parv[1], user_id)
	};

	const m::user::room user_room
	{
		user_id
	};

	//TODO: reuse composition from /status
	std::vector<json::value> list;
	return resource::response
	{
		client, json::value
		{
			list.data(), list.size()
		}
	};
}

//
// POST ?
//

static resource::response
post__presence(client &,
               const resource::request &);

resource::method
method_post
{
	presence_resource, "POST", post__presence,
	{
		method_post.REQUIRES_AUTH
	}
};

static resource::response
post__presence_list(client &,
                    const resource::request &);

resource::response
post__presence(client &client,
               const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"command required"
		};

	if(request.parv[0] == "list")
		return get__presence_list(client, request);

	throw m::NOT_FOUND
	{
		"Presence command not found"
	};
}

resource::response
post__presence_list(client &client,
                    const resource::request &request)
{
	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"user_id required"
		};

	m::user::id::buf user_id
	{
		url::decode(request.parv[1], user_id)
	};

	const m::user::room user_room
	{
		user_id
	};

	return resource::response
	{
		client, http::OK
	};
}

//
// put
//

static resource::response
put__presence_status(client &,
                     const resource::request &,
                     const m::user::id &);

static resource::response
put__presence(client &,
              const resource::request &);

resource::method
method_put
{
	presence_resource, "PUT", put__presence,
	{
		method_put.REQUIRES_AUTH
	}
};

resource::response
put__presence(client &client,
              const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"user_id required"
		};

	m::user::id::buf user_id
	{
		url::decode(request.parv[0], user_id)
	};

	if(user_id != request.user_id)
		throw m::FORBIDDEN
		{
			"You cannot set the presence of '%s' when you are '%s'",
			user_id,
			request.user_id
		};

	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"command required"
		};

	const auto &cmd
	{
		request.parv[1]
	};

	if(cmd == "status")
		return put__presence_status(client, request, user_id);

	throw m::NOT_FOUND
	{
		"Presence command not found"
	};
}

resource::response
put__presence_status(client &client,
                     const resource::request &request,
                     const m::user::id &user_id)
{
	const string_view &presence
	{
		unquote(request.at("presence"))
	};

	if(!m::presence::valid_state(presence))
		throw m::UNSUPPORTED
		{
			"That presence state is not supported"
		};

	const string_view &status_msg
	{
		trunc(unquote(request["status_msg"]), 390)
	};

	const m::user user
	{
		request.user_id
	};

	bool modified{true};
	m::presence::get(std::nothrow, user, [&modified, &presence, &status_msg]
	(const json::object &object)
	{
		if(unquote(object.get("presence")) != presence)
			return;

		if(unquote(object.get("status_msg")) != status_msg)
			return;

		modified = false;
	});

	if(!modified)
		return resource::response
		{
			client, http::OK
		};

	const auto eid
	{
		m::presence::set(user, presence, status_msg)
	};

	return resource::response
	{
		client, http::OK
	};
}

static void
handle_my_presence_changed(const m::event &event)
{
	if(!my(event))
		return;

	const m::user::id &user_id
	{
		json::get<"sender"_>(event)
	};

	if(!my(user_id))
		return;

	// The event has to be an ircd.presence in the user's room, not just a
	// random ircd.presence typed event in some other room...
	const m::user::room user_room{user_id};
	if(json::get<"room_id"_>(event) != user_room.room_id)
		return;

}

const m::hookfn<>
my_presence_changed
{
	handle_my_presence_changed,
	{
		{ "_site",  "vm.notify"      },
		{ "type",   "ircd.presence"  },
	}
};
