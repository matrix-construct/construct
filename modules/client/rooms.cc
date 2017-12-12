/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
	const m::vm::query<m::vm::where::equal> query
	{
		{ "room_id",    room_id  },
		{ "state_key",  nullptr  },
	};

	const size_t count
	{
		std::min(m::vm::count(query), 128UL)
	};

	if(!count && !exists(room_id))
		throw m::NOT_FOUND
		{
			"No messages."
		};

	size_t j(0);
	std::vector<json::value> ret(count);
	m::vm::for_each(query, [&count, &j, &ret]
	(const auto &event)
	{
		if(j < count)
		{
			if(!defined(json::get<"state_key"_>(event)))
				ret[j++] = event;
		}
	});

	return resource::response
	{
		client, json::members
		{
			{ "chunk", json::value { ret.data(), j } }
		}
	};
}

resource::response
get_members(client &client,
            const resource::request &request,
            const m::room::id &room_id)
{

	const m::vm::query<m::vm::where::equal> query
	{
		{ "room_id",    room_id         },
		{ "type",       "m.room.member" },
	};

	const auto count
	{
		m::vm::count(query)
	};

	if(!count)
		throw m::NOT_FOUND
		{
			"No members."
		};

	size_t j(0);
	std::vector<json::value> ret(count);
	m::vm::for_each(query, [&count, &j, &ret]
	(const auto &event)
	{
		if(j < count)
			ret[j++] = event;
	});

	return resource::response
	{
		client, json::members
		{
			{ "chunk", json::value { ret.data(), j } }
		}
	};
}

resource::response
get_state(client &client,
          const resource::request &request,
          const m::vm::query<> &query)
{
	const auto count
	{
		m::vm::count(query)
	};

	if(!count)
		throw m::NOT_FOUND
		{
			"No state."
		};

	size_t j(0);
	std::vector<json::value> ret(count);
	m::vm::for_each(query, [&count, &j, &ret]
	(const auto &event)
	{
		if(j < count)
		{
			if(defined(json::get<"state_key"_>(event)))
				ret[j++] = event;
		}
	});

	return resource::response
	{
		client, json::value
		{
			ret.data(), j
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
	const m::vm::query<m::vm::where::equal> query
	{
		{ "room_id",    room_id    },
		{ "event_id",   event_id   },
		{ "type",       type       },
		{ "state_key",  state_key  },
	};

	return get_state(client, request, query);
}

resource::response
get_state(client &client,
          const resource::request &request,
          const m::room::id &room_id,
          const string_view &event_id,
          const string_view &type)
{
	const m::vm::query<m::vm::where::equal> query
	{
		{ "room_id",    room_id    },
		{ "event_id",   event_id   },
		{ "type",       type       },
	};

	return get_state(client, request, query);
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

	const m::vm::query<m::vm::where::equal> query
	{
		{ "room_id",    room_id    },
		{ "event_id",   event_id   },
	};

	return get_state(client, request, query);
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

	json::iov event;

	const json::iov::push _type
	{
		event, { "type", type }
	};

	const json::iov::push _sender
	{
		event, { "sender", request.user_id }
	};

	const json::iov::push _content
	{
		event, { "content", json::object{request} }
	};

	m::room room
	{
		room_id
	};

	const auto event_id
	{
		room.send(event)
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
