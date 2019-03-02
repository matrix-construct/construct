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
	"Client 14.17.1.1 :Room Previews"
};

static bool
get_events_from(client &client,
                const resource::request &request,
                const m::room::id &room_id,
                const m::event::id &event_id,
                const m::event::id &room_head,
                json::stack::object &out);

static resource::response
get__events(client &client,
            const resource::request &request);

resource
events_resource
{
	"/_matrix/client/r0/events",
	{
		"(14.17.1.1) Room Previews"
	}
};

conf::item<ircd::milliseconds>
timeout_max
{
	{ "name",     "ircd.client.events.timeout.max"  },
	{ "default",  15 * 1000L                        },
};

conf::item<ircd::milliseconds>
timeout_min
{
	{ "name",     "ircd.client.events.timeout.min"  },
	{ "default",  5 * 1000L                         },
};

ircd::conf::item<ircd::milliseconds>
timeout_default
{
	{ "name",     "ircd.client.events.timeout.default"  },
	{ "default",  10 * 1000L                            },
};

static conf::item<size_t>
events_limit
{
	{ "name",      "ircd.client.rooms.events.limit" },
	{ "default",   32L                              },
};

static conf::item<size_t>
buffer_size
{
	{ "name",     "ircd.client.rooms.events.buffer_size" },
	{ "default",  long(96_KiB)                           },
};

static conf::item<size_t>
flush_hiwat
{
	{ "name",     "ircd.client.rooms.events.flush.hiwat" },
	{ "default",  long(16_KiB)                           },
};

resource::method
method_get
{
	events_resource, "GET", get__events
};

struct waiter
{
	m::user::id user_id;
	m::room::id room_id;
	std::string *event;
	ctx::dock *dock;
};

std::list<waiter>
clients;

resource::response
get__events(client &client,
            const resource::request &request)
{
	if(!request.query["room_id"])
		throw m::UNSUPPORTED
		{
			"Specify a room_id or use /sync"
		};

	m::room::id::buf room_id
	{
		url::decode(room_id, request.query.at("room_id"))
	};

	m::event::id::buf event_id
	{
		request.query["from"]?
			url::decode(event_id, request.query.at("from")):
			m::head(room_id)
	};

	const m::room room
	{
		room_id, event_id
	};

	if(!room.visible(request.user_id))
		throw m::ACCESS_DENIED
		{
			"You are not able to view the room at this event."
		};

	resource::response::chunked response
	{
		client, http::OK, buffer_size
	};

	json::stack out
	{
		response.buf, response.flusher(), size_t(flush_hiwat)
	};

	json::stack::object top
	{
		out
	};

	const m::event::id::buf room_head
	{
		m::head(room_id)
	};

	json::stack::member
	{
		top, "start", event_id
	};

	if(event_id && event_id != room_head)
	{
		json::stack::checkpoint checkpoint
		{
			out
		};

		if(get_events_from(client, request, room_id, event_id, room_head, top))
			return response;

		checkpoint.rollback();
	}

	ctx::dock dock;
	std::string event;
	const unique_iterator it
	{
		clients, clients.emplace(end(clients), waiter{request.user_id, room_id, &event, &dock})
	};

	const milliseconds timeout{[&request]
	{
		const milliseconds _default(timeout_default);
		milliseconds ret(request.query.get("timeout", _default));
		ret = std::max(ret, milliseconds(timeout_min));
		ret = std::min(ret, milliseconds(timeout_max));
		return ret;
	}()};

	dock.wait_for(timeout, [&event]
	{
		return !empty(event);
	});

	if(!event.empty())
	{
		json::stack::array chunk
		{
			top, "chunk"
		};

		json::stack::object object
		{
			chunk
		};

		object.append(json::object(event));
		json::stack::object unsigned_
		{
			object, "unsigned"
		};

		json::stack::member
		{
			unsigned_, "age", json::value{0L}
		};
	}
	else json::stack::array
	{
		top, "chunk"
	};

	if(json::object(event).has("event_id"))
		json::stack::member
		{
			top, "end", unquote(json::object(event).get("event_id"))
		};
	else
		json::stack::member
		{
			top, "end", room_head
		};

	return response;
}

static void
handle_notify(const m::event &,
              m::vm::eval &);

m::hookfn<m::vm::eval &>
notified
{
	handle_notify,
	{
		{ "_site",  "vm.notify" },
	}
};

void
handle_notify(const m::event &event,
              m::vm::eval &)
{
	const auto &room_id
	{
		json::get<"room_id"_>(event)
	};

	if(!room_id)
		return;

	for(auto &waiter : clients)
	{
		if(!waiter.event->empty())
			continue;

		if(waiter.room_id != room_id)
			continue;

		assert(waiter.event);
		*waiter.event = json::strung{event};

		assert(waiter.dock);
		waiter.dock->notify_one();
	}
}

bool
get_events_from(client &client,
                const resource::request &request,
                const m::room::id &room_id,
                const m::event::id &event_id,
                const m::event::id &room_head,
                json::stack::object &out)
{
	m::room::messages it
	{
		room_id, event_id
	};

	if(!it)
		return false;

	json::stack::array chunk
	{
		out, "chunk"
	};

	size_t i(0), j(0);
	for(; it && i < size_t(events_limit); --it, ++i)
	{
		if(!visible(it.event_id(), request.user_id))
			continue;
		else
			++j;

		json::stack::object object
		{
			chunk
		};

		object.append(*it);
		json::stack::object unsigned_
		{
			object, "unsigned"
		};

		json::stack::member
		{
			unsigned_, "age", json::value
			{
				 long(m::vm::current_sequence - it.event_idx())
			}
		};
	}

	if(!j)
		return j;

	chunk.~array();
	json::stack::member
	{
		out, "end", it? it.event_id() : room_head
	};

	return j;
}
