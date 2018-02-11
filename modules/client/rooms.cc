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

struct room
:resource
{
	using resource::resource;
}
rooms_resource
{
	"/_matrix/client/r0/rooms/", resource::opts
	{
		"Rooms (7.0)",
		resource::DIRECTORY,
	}
};

mapi::header IRCD_MODULE
{
	"registers the resource 'client/rooms'"
};

resource::response
get_messages(client &client,
             const resource::request &request,
             const m::room::id &room_id)
{
	const string_view &from
	{
		request.query.at("from")
	};

	const string_view &to
	{
		request.query["to"]
	};

	const char &dir
	{
		request.query.at("dir").at(0)
	};

	const uint16_t limit
	{
		request.query["limit"]? lex_cast<uint16_t>(request.query.at("limit")) : ushort(10)
	};

	const string_view &filter
	{
		request.query["filter"]
	};

	const m::room room
	{
		room_id
	};

	const m::room::messages messages
	{
		room
	};

	std::vector<json::value> ret;
	ret.reserve(limit);
	messages.test([&limit, &ret](const m::event &event)
	{
		ret.emplace_back(event);
		return ret.size() >= limit;
	});

	return resource::response
	{
		client, json::members
		{
			{ "chunk", json::value { ret.data(), ret.size() } }
		}
	};
}

resource::response
get_members(client &client,
            const resource::request &request,
            const m::room::id &room_id)
{
	const m::room room
	{
		room_id
	};

	const m::room::members members
	{
		room
	};

	//TODO: Introduce the progressive json::stream with socket <-> db yield dialectic.
	std::vector<json::value> ret;
	ret.reserve(2048); // 2048 * 16 bytes... good enuf atm

	members.for_each([&ret](const m::event &event)
	{
		ret.emplace_back(event);
	});

	return resource::response
	{
		client, json::members
		{
			{ "chunk", json::value { ret.data(), ret.size() } }
		}
	};
}

resource::response
get_state(client &client,
          const resource::request &request,
          const m::room::id &room_id,
          const string_view &event_id)
{
	const m::room::state state
	{
		m::room{room_id, event_id}
	};

	//TODO: Introduce the progressive json::stream with socket <-> db yield dialectic.
	std::vector<json::value> ret;
	ret.reserve(2048); // 2048 * 16 bytes... good enuf atm

	state.for_each([&ret]
	(const m::event &event)
	{
		ret.emplace_back(event);
	});

	return resource::response
	{
		client, json::value
		{
			ret.data(), ret.size()
		}
	};
}

resource::response
get_state(client &client,
          const resource::request &request,
          const m::room::id &room_id,
          const string_view &event_id,
          const string_view &type)
{
	const m::room::state state
	{
		m::room{room_id, event_id}
	};

	//TODO: Introduce the progressive json::stream with socket <-> db yield dialectic.
	std::vector<json::value> ret;
	ret.reserve(2048); // 2048 * 16 bytes... good enuf atm
	state.for_each(type, [&ret]
	(const m::event &event)
	{
		//TODO: Fix conversion derpage
		ret.emplace_back(event);
	});

	return resource::response
	{
		client, json::value
		{
			ret.data(), ret.size()
		}
	};
}

resource::response
get_state(client &client,
          const resource::request &request,
          const m::room::id &room_id,
          const string_view &event_id,
          const string_view &type,
          const string_view &state_key)
{
	const m::room::state state
	{
		m::room{room_id, event_id}
	};

	std::array<json::value, 1> ret;
	const bool i{state.get(std::nothrow, type, state_key, [&ret]
	(const m::event &event)
	{
		ret[0] = event;
	})};

	return resource::response
	{
		client, json::value
		{
			ret.data(), i
		}
	};
}

resource::response
get_state(client &client,
          const resource::request &request,
          const m::room::id &room_id)
{
	char type_buf[uint(256 * 1.34 + 1)];
	const string_view &type
	{
		url::decode(request.parv[2], type_buf)
	};

	char skey_buf[uint(256 * 1.34 + 1)];
	const string_view &state_key
	{
		url::decode(request.parv[3], skey_buf)
	};

	// (non-standard) Allow an event_id to be passed in the query string
	// for reference framing.
	char evid_buf[uint(256 * 1.34 + 1)];
	const string_view &event_id
	{
		url::decode(request.query["event_id"], evid_buf)
	};

	if(type && state_key)
		return get_state(client, request, room_id, event_id, type, state_key);

	if(type)
		return get_state(client, request, room_id, event_id, type);

	return get_state(client, request, room_id, event_id);
}

resource::response
get_context(client &client,
            const resource::request &request,
            const m::room::id &room_id)
{
	m::event::id::buf event_id
	{
		url::decode(request.parv[2], event_id)
	};

	const m::vm::query<m::vm::where::equal> query
	{
		{ "room_id",   room_id   },
		{ "event_id",  event_id  },
	};

	std::string ret;
	const bool found
	{
		m::vm::test(query, [&ret]
		(const m::event &event)
		{
			ret = json::strung{event};
			return true;
		})
	};

	if(!found)
		throw m::NOT_FOUND{"event not found"};

	return resource::response
	{
		client, json::members
		{
			{ "event", ret }
		}
	};
}

resource::response
get_rooms(client &client, const resource::request &request)
{
	if(request.parv.size() < 2)
		throw m::error
		{
			http::MULTIPLE_CHOICES, "M_NOT_FOUND", "/rooms command required"
		};

	m::room::id::buf room_id
	{
		url::decode(request.parv[0], room_id)
	};

	const string_view &cmd
	{
		request.parv[1]
	};

	if(cmd == "context")
		return get_context(client, request, room_id);

	if(cmd == "state")
		return get_state(client, request, room_id);

	if(cmd == "members")
		return get_members(client, request, room_id);

	if(cmd == "messages")
		return get_messages(client, request, room_id);

	throw m::NOT_FOUND
	{
		"/rooms command not found"
	};
}

resource::method method_get
{
	rooms_resource, "GET", get_rooms
};

resource::response
put_send(client &client,
         const resource::request &request,
         const m::room::id &room_id)
{
	if(request.parv.size() < 3)
		throw m::BAD_REQUEST{"type parameter missing"};

	const string_view &type
	{
		request.parv[2]
	};

	if(request.parv.size() < 4)
		throw m::BAD_REQUEST{"txnid parameter missing"};

	const string_view &txnid
	{
		request.parv[3]
	};

	m::room room
	{
		room_id
	};

	const auto event_id
	{
		send(room, request.user_id, type, json::object{request})
	};

	return resource::response
	{
		client, json::members
		{
			{ "event_id", event_id }
		}
	};
}

resource::response
put_typing(client &client,
           const resource::request &request,
           const m::room::id &room_id)
{
	if(request.parv.size() < 3)
		throw m::BAD_REQUEST{"user_id parameter missing"};

	m::user::id::buf user_id
	{
		url::decode(request.parv[2], user_id)
	};

	static const milliseconds timeout_default
	{
		30 * 1000
	};

	const auto timeout
	{
		request.get("timeout", timeout_default)
	};

	const auto typing
	{
		request.at<bool>("typing")
	};

	log::debug("%s typing: %d timeout: %ld",
	           user_id,
	           typing,
	           timeout.count());

	return resource::response
	{
		client, http::OK
	};
}

resource::response
put_rooms(client &client, const resource::request &request)
{
	if(request.parv.size() < 2)
		throw m::BAD_REQUEST{"/rooms command required"};

	m::room::id::buf room_id
	{
		url::decode(request.parv[0], room_id)
	};

	const string_view &cmd
	{
		request.parv[1]
	};

	if(cmd == "send")
		return put_send(client, request, room_id);

	if(cmd == "typing")
		return put_typing(client, request, room_id);

	throw m::NOT_FOUND{"/rooms command not found"};
}

resource::method method_put
{
	rooms_resource, "PUT", put_rooms,
	{
		method_put.REQUIRES_AUTH
	}
};

resource::response
post_receipt(client &client,
             const resource::request &request,
             const m::room::id &room_id)
{
	if(request.parv.size() < 4)
		throw m::BAD_REQUEST{"receipt type and event_id required"};

	const string_view &receipt_type{request.parv[2]};
	const string_view &event_id{request.parv[3]};
	//std::cout << "type: " << receipt_type << " eid: " << event_id << std::endl;
	return {};
}

resource::response
post_join(client &client,
          const resource::request &request,
          const m::room::id &room_id)
{
	m::join(room_id, request.user_id);

	return resource::response
	{
		client, json::members
		{
			{ "room_id", room_id }
		}
	};
}

resource::response
post_rooms(client &client,
           const resource::request &request)
{
	if(request.parv.size() < 2)
		throw m::BAD_REQUEST{"/rooms command required"};

	m::room::id::buf room_id
	{
		url::decode(request.parv[0], room_id)
	};

	const string_view &cmd
	{
		request.parv[1]
	};

	if(cmd == "receipt")
		return post_receipt(client, request, room_id);

	if(cmd == "join")
		return post_join(client, request, room_id);

	throw m::error
	{
		http::MULTIPLE_CHOICES, "M_NOT_FOUND", "/rooms command required"
	};
}

resource::method method_post
{
	rooms_resource, "POST", post_rooms,
	{
		method_post.REQUIRES_AUTH
	}
};
