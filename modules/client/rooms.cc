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
		resource::DIRECTORY,
		"Rooms (7.0)"
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
	const m::event::query<m::event::where::equal> event_in_room
	{
		{ "room_id", room_id }
	};

	const m::event::query<m::event::where::test> event_not_state
	{
		[](const auto &event)
		{
			return !defined(json::get<"state_key"_>(event));
		}
	};

	const auto query
	{
		event_in_room && event_not_state
	};

	const size_t count
	{
		std::min(m::events::count(query), 128UL)
	};

	if(!count)
		throw m::NOT_FOUND
		{
			"No messages."
		};

	size_t j(0);
	json::value ret[count];
	m::events::for_each(query, [&count, &j, &ret]
	(const auto &event)
	{
		if(j < count)
			ret[j++] = event;
	});

	return resource::response
	{
		client, json::members
		{
			{ "chunk", json::value { ret, j } }
		}
	};
}

resource::response
get_members(client &client,
            const resource::request &request,
            const m::room::id &room_id)
{

	const m::event::query<m::event::where::equal> query
	{
		{ "room_id",    room_id         },
		{ "type",       "m.room.member" },
		{ "state_key",  ""              },
	};

	const auto count
	{
		m::events::count(query)
	};

	if(!count)
		throw m::NOT_FOUND
		{
			"No members."
		};

	size_t j(0);
	json::value ret[count];
	m::events::for_each(query, [&count, &j, &ret]
	(const auto &event)
	{
		if(j < count)
			ret[j++] = event;
	});

	return resource::response
	{
		client, json::members
		{
			{ "chunk", json::value { ret, j } }
		}
	};
}

resource::response
get_state(client &client,
          const resource::request &request,
          const m::event::query<> &query)
{
	const auto count
	{
		m::events::count(query)
	};

	if(!count)
		throw m::NOT_FOUND
		{
			"No state."
		};

	size_t j(0);
	json::value ret[count];
	m::events::for_each(query, [&count, &j, &ret]
	(const auto &event)
	{
		if(j < count)
			ret[j++] = event;
	});

	return resource::response
	{
		client, json::value
		{
			ret, j
		}
	};
}

resource::response
get_state(client &client,
          const resource::request &request,
          const m::room::id &room_id,
          const string_view &type,
          const string_view &state_key)
{
	const m::event::query<m::event::where::equal> query
	{
		{ "room_id",    room_id    },
		{ "type",       type       },
		{ "state_key",  state_key  },
	};

	return get_state(client, request, query);
}

resource::response
get_state(client &client,
          const resource::request &request,
          const m::room::id &room_id,
          const string_view &type)
{
	const m::event::query<m::event::where::equal> query
	{
		{ "room_id",    room_id    },
		{ "type",       type       }
	};

	return get_state(client, request, query);
}

resource::response
get_state(client &client,
          const resource::request &request,
          const m::room::id &room_id)
{
	const string_view &type
	{
		request.parv[2]
	};

	const string_view &state_key
	{
		request.parv[3]
	};

	if(type && state_key)
		return get_state(client, request, room_id, type, state_key);

	if(type)
		return get_state(client, request, room_id, type);

	const m::event::query<m::event::where::equal> query
	{
		{ "room_id",    room_id    },
		{ "state_key",  ""         },
	};

	return get_state(client, request, query);
}

resource::response
get_context(client &client,
            const resource::request &request,
            const m::room::id &room_id)
{
	const m::event::id &event_id
	{
		request.parv[2]
	};

	const auto it
	{
		m::event::find(event_id)
	};

	if(!it)
		throw m::NOT_FOUND{"event not found"};

	const auto event
	{
		json::string(*it)
	};

	return resource::response
	{
		client, json::members
		{
			{ "event", event }
		}
	};
}

resource::response
get_rooms(client &client, const resource::request &request)
{
	if(request.parv.size() != 2)
		throw m::error
		{
			http::MULTIPLE_CHOICES, "/rooms command required"
		};

	m::room::id::buf room_id
	{
		urldecode(request.parv[0], room_id)
	};

	const string_view &cmd{request.parv[1]};

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
	const string_view &type
	{
		request.parv[2]
	};

	const string_view &txnid
	{
		request.parv[3]
	};

	json::iov event;

	const json::iov::push _type
	{
		event, "type", type
	};

	const json::iov::push _sender
	{
		event, "sender", request.user_id
	};

	const json::iov::push _content
	{
		event, "content", json::object{request}
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
put_rooms(client &client, const resource::request &request)
{
	if(request.parv.size() != 2)
		throw m::BAD_REQUEST{"/rooms command required"};

	m::room::id::buf room_id
	{
		urldecode(request.parv[0], room_id)
	};

	const string_view &cmd
	{
		request.parv[1]
	};

	if(cmd == "send")
		return put_send(client, request, room_id);

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
	if(request.parv.size() != 4)
		throw m::BAD_REQUEST{"receipt type and event_id required"};

	const string_view &receipt_type{request.parv[2]};
	const string_view &event_id{request.parv[3]};
	std::cout << "type: " << receipt_type << " eid: " << event_id << std::endl;
}

resource::response
post_rooms(client &client,
           const resource::request &request)
{
	if(request.parv.size() != 2)
		throw m::BAD_REQUEST{"/rooms command required"};

	m::room::id::buf room_id
	{
		urldecode(request.parv[0], room_id)
	};

	const string_view &cmd
	{
		request.parv[1]
	};

	if(cmd == "receipt")
		return post_receipt(client, request, room_id);
}

resource::method method_post
{
	rooms_resource, "POST", post_rooms,
	{
		method_post.REQUIRES_AUTH
	}
};
