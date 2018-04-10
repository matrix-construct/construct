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
	"Client 6.2.3 :initialSync"
};

const string_view
initialsync_description
{R"(

6.2.3

This returns the full state for this user, with an optional limit on the number
of messages per room to return.

This endpoint was deprecated in r0 of this specification. Clients should instead
call the /sync API with no since parameter.

*** developer note:
We reuse the routines of this module for the initial sync portion of the /sync
API, and branch for their spec differences when applicable. This way the /sync
module focuses specifically on the increment aspect.

)"};

//
// sync resource
//

resource
initialsync_resource
{
	"/_matrix/client/r0/initialSync",
	{
		initialsync_description
	}
};

extern "C" resource::response
initialsync(client &client,
            const resource::request &request);

std::string
initialsync_rooms(client &client,
                  const resource::request &request,
                  const string_view &filter_id);

resource::method
get_initialsync
{
	initialsync_resource, "GET", initialsync,
	{
		get_initialsync.REQUIRES_AUTH
	}
};

conf::item<size_t>
initialsync_limit_default
{
	{ "name",     "ircd.client.initialsync.limit.default" },
	{ "default",  16L                                     },
};

conf::item<size_t>
initialsync_limit_max
{
	{ "name",     "ircd.client.initialsync.limit.max"     },
	{ "default",  64L                                     },
};

resource::response
initialsync(client &client,
            const resource::request &request)
{
	const auto &filter_id
	{
		request.query["filter"]
	};

	const auto &set_presence
	{
		request.query.get("set_presence", "online"_sv)
	};

	const auto &limit{[&request]
	{
		auto ret(request.query.get("limit", size_t(initialsync_limit_default)));
		ret = std::min(ret, size_t(initialsync_limit_max));
		return ret;
	}()};

	const std::string rooms
	{
		initialsync_rooms(client, request, filter_id)
	};

	const m::user::room ur
	{
		m::user::id{request.user_id}
	};

	std::vector<json::value> presents;
	ur.get(std::nothrow, "m.presence", [&]
	(const m::event &event)
	{
		presents.emplace_back(event);
	});

	const json::members presence
	{
		{ "events", json::value { presents.data(), presents.size() } },
	};

	const auto &state_key
	{
		request.access_token
	};

	//TODO: XXX
	const fmt::bsprintf<32> next_batch
	{
		"%zu", m::vm::current_sequence
	};

	const m::user::room user_room
	{
		request.user_id
	};

	m::send(user_room, request.user_id, "ircd.tape.head", state_key,
	{
		{ "sequence",  next_batch }
	});

	return resource::response
	{
		client, json::members
		{
			{ "next_batch",  next_batch  },
			{ "rooms",       rooms       },
			{ "presence",    presence    },
		}
	};
}

std::string
initialsync_room(client &client,
                 const resource::request &request,
                 const m::room &room);

std::string
initialsync_rooms(client &client,
                  const resource::request &request,
                  const string_view &filter_id)
{
	m::user user{request.user_id};
	m::user::room user_room{user};
	m::room::state user_state{user_room};

	std::array<std::vector<std::string>, 3> r;
	std::array<std::vector<json::member>, 3> m;

	// Get the rooms the user is a joined member in by iterating the state
	// events in the user's room.
	user_state.for_each("ircd.member", [&r, &m, &client, &request]
	(const m::event &event)
	{
		const m::room::id &room_id
		{
			unquote(at<"state_key"_>(event))
		};

		const auto &membership
		{
			unquote(at<"content"_>(event).at("membership"))
		};

		const auto i
		{
			membership == "join"? 0:
			membership == "leave"? 1:
			membership == "invite"? 2:
			0
		};

		r.at(i).emplace_back(initialsync_room(client, request, room_id));
		m.at(i).emplace_back(room_id, r.at(i).back());
	});

	const std::string join{json::strung(m[0].data(), m[0].data() + m[0].size())};
	const std::string leave{json::strung(m[1].data(), m[1].data() + m[1].size())};
	const std::string invite{json::strung(m[2].data(), m[2].data() + m[2].size())};
	return json::strung(json::members
	{
		{ "join",     join    },
		{ "leave",    leave   },
		{ "invite",   invite  },
	});
}

std::string
initialsync_room(client &client,
                 const resource::request &request,
                 const m::room &room)
{
	const unique_buffer<mutable_buffer> buf{40_MiB}; //TODO: XXXXXXXXXXXXXXXXXX
	json::stack out{buf};
	{
		json::stack::object _state_object
		{
			out
		};

		json::stack::member _state_events
		{
			_state_object, "events"
		};

		json::stack::array _state_events_a
		{
			_state_events
		};

		const m::room::state state_
		{
			room
		};

		state_.for_each([&_state_events_a](const m::event &event)
		{
			_state_events_a.append(event);
		});
	}

	const std::string state_serial
	{
		out.buf.completed()
	};

	out.clear();
	{
		json::stack::array _timeline_events_a
		{
			out
		};

		// messages seeks to the newest event, but the client wants the oldest
		// event first so we seek down first and then iterate back up. Due to
		// an issue with rocksdb's prefix-iteration this iterator becomes
		// toxic as soon as it becomes invalid. As a result we have to copy the
		// event_id on the way down in case of renewing the iterator for the
		// way back. This is not a big deal but rocksdb should fix their shit.
		ssize_t i(0);
		m::event::id::buf event_id;
		m::room::messages it{room};
		for(; it && i < 10; --it, ++i)
			event_id = it.event_id();

		if(i > 0 && !it)
			it.seek(event_id);

		if(i > 0)
			for(; it && i > -1; ++it, --i)
				_timeline_events_a.append(*it);
	}

	const std::string timeline_serial
	{
		out.buf.completed()
	};

	std::vector<std::string> ephemeral;
	const json::strung ephemeral_serial
	{
		ephemeral.data(), ephemeral.data() + ephemeral.size()
	};

	const auto &prev_batch
	{
		!json::array{timeline_serial}.empty()?
			unquote(json::object{json::array{timeline_serial}.at(0)}.get("event_id")):
			string_view{}
	};

	const json::members body
	{
		{ "account_data", json::members{} },
		{ "unread_notifications",
		{
			{ "highlight_count", int64_t(0) },
			{ "notification_count", int64_t(0) },
		}},
		{ "ephemeral",
		{
			{ "events", ephemeral_serial },
		}},
		{ "state", state_serial },
		{ "timeline",
		{
			{ "events", timeline_serial },
			{ "prev_batch", prev_batch },
			{ "limited", false },                       //TODO: XXX
		}},
	};

	return json::strung(body);
}
