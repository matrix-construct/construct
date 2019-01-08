// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "sync.h"

ircd::mapi::header
IRCD_MODULE
{
	"Client 6.2.1 :Sync"
};

decltype(ircd::m::sync::resource)
ircd::m::sync::resource
{
	"/_matrix/client/r0/sync",
	{
		description
	}
};

decltype(ircd::m::sync::description)
ircd::m::sync::description
{R"(6.2.1

Synchronise the client's state with the latest state on the server. Clients
use this API when they first log in to get an initial snapshot of the state
on the server, and then continue to call this API to get incremental deltas
to the state, and to receive new messages.
)"};

ircd::conf::item<ircd::milliseconds>
ircd::m::sync::args::timeout_max
{
	{ "name",     "ircd.client.sync.timeout.max"  },
	{ "default",  15 * 1000L                      },
};

ircd::conf::item<ircd::milliseconds>
ircd::m::sync::args::timeout_min
{
	{ "name",     "ircd.client.sync.timeout.min"  },
	{ "default",  5 * 1000L                       },
};

ircd::conf::item<ircd::milliseconds>
ircd::m::sync::args::timeout_default
{
	{ "name",     "ircd.client.sync.timeout.default"  },
	{ "default",  10 * 1000L                          },
};

//
// GET sync
//

decltype(ircd::m::sync::method_get)
ircd::m::sync::method_get
{
	resource, "GET", handle_get,
	{
		method_get.REQUIRES_AUTH,
		-1s,
	}
};

ircd::resource::response
ircd::m::sync::handle_get(client &client,
                          const resource::request &request)
try
{
	const args args
	{
		request
	};

	const std::pair<event::idx, event::idx> range
	{
		args.since,                  // start at since token
		m::vm::current_sequence      // stop at present
	};

	stats stats;
	data data
	{
		stats, client, request.user_id, range, args.filter_id
	};

	log::debug
	{
		log, "request %s", loghead(data)
	};

	if(data.since > data.current + 1)
		throw m::NOT_FOUND
		{
			"Since parameter is too far in the future..."
		};

	const size_t linear_delta_max
	{
		linear::delta_max
	};

	const bool shortpolled
	{
		range.first > range.second?
			false:
		range.second - range.first <= linear_delta_max?
			linear::handle(data):
			polylog::handle(data)
	};

	// When shortpoll was successful, do nothing else.
	if(shortpolled)
		return {};

	// When longpoll was successful, do nothing else.
	if(longpoll::poll(client, data, args))
		return {};

	// A user-timeout occurred. According to the spec we return a
	// 200 with empty fields rather than a 408.
	const json::value next_batch
	{
		lex_cast(m::vm::current_sequence + 1), json::STRING
	};

	return resource::response
	{
		client, json::members
		{
			{ "next_batch",  next_batch      },
			{ "rooms",       json::object{}  },
			{ "presence",    json::object{}  },
		}
	};
}
catch(const bad_lex_cast &e)
{
	throw m::BAD_REQUEST
	{
		"Since parameter invalid :%s", e.what()
	};
}

//
// polylog
//

bool
ircd::m::sync::polylog::handle(data &data)
try
{
	data.commit();

	// Top level sync object.
	json::stack::object object
	{
		data.out
	};

	m::sync::for_each([&data]
	(item &item)
	{
		json::stack::member member
		{
			data.out, item.member_name()
		};

		item.polylog(data);
		return true;
	});

	const json::value next_batch
	{
		lex_cast(data.current + 1), json::STRING
	};

	json::stack::member
	{
		object, "next_batch", next_batch
	};

	log::info
	{
		log, "polylog %s (next_batch:%s)",
		loghead(data),
		string_view{next_batch}
	};

	return true;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "polylog %s FAILED :%s",
		loghead(data),
		e.what()
	};

	throw;
}

//
// longpoll
//

decltype(ircd::m::sync::longpoll::notified)
ircd::m::sync::longpoll::notified
{
	handle_notify,
	{
		{ "_site",  "vm.notify" },
	}
};

void
ircd::m::sync::longpoll::handle_notify(const m::event &event,
                                       m::vm::eval &eval)
{
	assert(eval.opts);
	if(!eval.opts->notify_clients)
		return;

	if(!polling)
	{
		queue.clear();
		return;
	}

	queue.emplace_back(eval);
	dock.notify_all();
}

bool
ircd::m::sync::longpoll::poll(client &client,
                              data &data,
                              const args &args)
{
	++polling;
	const unwind unpoll{[]
	{
		--polling;
	}};

	while(1)
	{
		if(!dock.wait_until(args.timesout))
			return false;

		if(queue.empty())
			continue;

		const auto &a(queue.front());
		const unwind pop{[]
		{
			if(polling <= 1)
				queue.pop_front();
		}};

		if(handle(client, data, args, a))
			return true;
	}
}

bool
ircd::m::sync::longpoll::handle(client &client,
                                data &data,
                                const args &args,
                                const accepted &event)
{
	const auto &room_id
	{
		json::get<"room_id"_>(event)
	};

	if(room_id)
	{
		const m::room room{room_id};
		return handle(client, data, args, event, room);
	}

	return false;
}

bool
ircd::m::sync::longpoll::handle(client &client,
                                data &data,
                                const args &args,
                                const accepted &event,
                                const m::room &room)
{
	const m::user::id &user_id
	{
		args.request.user_id
	};

	if(!room.membership(user_id, "join"))
		return false;

	const auto rooms
	{
		sync_rooms(client, user_id, room, args, event)
	};

	const m::user::room ur
	{
		m::user::id
		{
			args.request.user_id
		}
	};

	std::vector<json::value> presents;
	ur.get(std::nothrow, "ircd.presence", [&]
	(const m::event &event)
	{
		const auto &content
		{
			at<"content"_>(event)
		};

		presents.emplace_back(event);
	});

	const json::members presence
	{
		{ "events", json::value { presents.data(), presents.size() } },
	};

	const auto &next_batch
	{
		int64_t(data.current + 1)
	};

	resource::response
	{
		client, json::members
		{
			{ "next_batch",  json::value { lex_cast(next_batch), json::STRING } },
			{ "rooms",       rooms       },
			{ "presence",    presence    },
		}
	};

	return true;
}

std::string
ircd::m::sync::longpoll::sync_rooms(client &client,
                                    const m::user::id &user_id,
                                    const m::room &room,
                                    const args &args,
                                    const accepted &event)
{
	std::vector<std::string> r;
	std::vector<json::member> m;

	thread_local char membership_buf[64];
	const auto membership
	{
		room.membership(membership_buf, user_id)
	};

	r.emplace_back(sync_room(client, room, args, event));
	m.emplace_back(room.room_id, r.back());

	const json::strung body
	{
		m.data(), m.data() + m.size()
	};

	return json::strung{json::members
	{
		{ membership, body }
	}};
}

std::string
ircd::m::sync::longpoll::sync_room(client &client,
                                   const m::room &room,
                                   const args &args,
                                   const accepted &accepted)
{
	const auto &since
	{
		args.since
	};

	const m::event &event{accepted};
	std::vector<std::string> timeline;
	if(defined(json::get<"event_id"_>(event)))
	{
		json::strung strung(event);
		if(!!accepted.client_txnid)
			strung = json::insert(strung, json::member
			{
				"unsigned", json::members
				{
					{ "transaction_id", accepted.client_txnid }
				}
			});

		timeline.emplace_back(std::move(strung));
	}

	const json::strung timeline_serial
	{
		timeline.data(), timeline.data() + timeline.size()
	};

	std::vector<std::string> ephemeral;
	if(json::get<"type"_>(event) == "m.typing") //TODO: X
		ephemeral.emplace_back(json::strung{event});

	if(json::get<"type"_>(event) == "m.receipt") //TODO: X
		ephemeral.emplace_back(json::strung(event));

	const json::strung ephemeral_serial
	{
		ephemeral.data(), ephemeral.data() + ephemeral.size()
	};

	const auto &prev_batch
	{
		!timeline.empty()?
			unquote(json::object{timeline.front()}.get("event_id")):
			string_view{}
	};

	//TODO: XXX
	const bool limited
	{
		false
	};

	m::event::id::buf last_read_buf;
	const auto &last_read
	{
		m::receipt::read(last_read_buf, room.room_id, args.request.user_id)
	};

	const auto last_read_idx
	{
		last_read && json::get<"event_id"_>(event)?
			index(last_read):
			0UL
	};

	const auto current_idx
	{
		last_read_idx?
			index(at<"event_id"_>(event)):
			0UL
	};

	const auto notes
	{
		last_read_idx?
			notification_count(room, last_read_idx, current_idx):
			json::undefined_number
	};

	const auto highlights
	{
		last_read_idx?
			highlight_count(room, args.request.user_id, last_read_idx, current_idx):
			json::undefined_number
	};

	const json::members body
	{
		{ "account_data", json::members{} },
		{ "unread_notifications",
		{
			{ "highlight_count",     highlights  },
			{ "notification_count",  notes       },
		}},
		{ "ephemeral",
		{
			{ "events", ephemeral_serial },
		}},
		{ "timeline",
		{
			{ "events",      timeline_serial  },
			{ "prev_batch",  prev_batch       },
			{ "limited",     limited          },
		}},
	};

	return json::strung(body);
}

//
// linear
//

decltype(ircd::m::sync::linear::delta_max)
ircd::m::sync::linear::delta_max
{
	{ "name",     "ircd.client.sync.linear.delta.max" },
	{ "default",  1024                                },
};

bool
ircd::m::sync::linear::handle(data &data)
{
	auto &client{data.client};
	json::stack::object object
	{
		data.out
	};

	uint64_t since
	{
		data.since
	};

	std::map<std::string, std::vector<std::string>, std::less<>> r;

	bool limited{false};
	m::events::for_each(since, [&]
	(const uint64_t &sequence, const m::event &event)
	{
		if(!r.empty() && (since - data.since > 128))
		{
			limited = true;
			return false;
		}

		since = sequence;
		if(!json::get<"room_id"_>(event))
			return true;

		const m::room room
		{
			json::get<"room_id"_>(event)
		};

		if(!room.membership(data.user.user_id))
			return true;

		auto it
		{
			r.lower_bound(room.room_id)
		};

		if(it == end(r) || it->first != room.room_id)
			it = r.emplace_hint(it, std::string{room.room_id}, std::vector<std::string>{});

		it->second.emplace_back(json::strung{event});
		return true;
	});

	if(r.empty())
		return false;

	std::vector<json::member> events[3];

	for(auto &p : r)
	{
		const m::room::id &room_id{p.first};
		auto &vec{p.second};

		std::vector<std::string> timeline;
		std::vector<std::string> state;
		std::vector<std::string> ephemeral;

		for(std::string &event : vec)
			if(json::object{event}.has("state_key"))
				state.emplace_back(std::move(event));
			else if(!json::object{event}.has("prev_events"))
				ephemeral.emplace_back(std::move(event));
			else
				timeline.emplace_back(std::move(event));

		const json::strung timeline_serial{timeline.data(), timeline.data() + timeline.size()};
		const json::strung state_serial{state.data(), state.data() + state.size()};
		const json::strung ephemeral_serial{ephemeral.data(), ephemeral.data() + ephemeral.size()};

		m::event::id::buf last_read_buf;
		const auto &last_read
		{
			m::receipt::read(last_read_buf, room_id, data.user)
		};

		const auto last_read_idx
		{
			last_read?
				index(last_read):
				0UL
		};

		const auto notes
		{
			last_read_idx?
				notification_count(room_id, last_read_idx, data.current):
				json::undefined_number
		};

		const auto highlights
		{
			last_read_idx?
				highlight_count(room_id, data.user, last_read_idx, data.current):
				json::undefined_number
		};

		const string_view prev_batch
		{
			!timeline.empty()?
				unquote(json::object{timeline.front()}.at("event_id")):
				string_view{}
		};

		const json::members body
		{
			{ "ephemeral",
			{
				{ "events", ephemeral_serial },
			}},
			{ "state",
			{
				{ "events", state_serial }
			}},
			{ "timeline",
			{
				{ "events",      timeline_serial  },
				{ "prev_batch",  prev_batch       },
				{ "limited",     limited          },
			}},
			{ "unread_notifications",
			{
				{ "highlight_count",     highlights  },
				{ "notification_count",  notes       },
			}},
		};

		thread_local char membership_buf[64];
		const auto membership{m::room{room_id}.membership(membership_buf, data.user)};
		const int ep
		{
			membership == "join"? 0:
			membership == "leave"? 1:
			membership == "invite"? 2:
			1 // default to leave (catches "ban" for now)
		};

		events[ep].emplace_back(room_id, body);
	};

	const json::value joinsv
	{
		events[0].data(), events[0].size()
	};

	const json::value leavesv
	{
		events[1].data(), events[1].size()
	};

	const json::value invitesv
	{
		events[2].data(), events[2].size()
	};

	const json::members rooms
	{
		{ "join",   joinsv     },
		{ "leave",  leavesv    },
		{ "invite", invitesv   },
	};

	resource::response
	{
		client, json::members
		{
			{ "next_batch",  json::value { lex_cast(int64_t(data.current + 1)), json::STRING } },
			{ "rooms",       rooms   },
			{ "presence",    json::object{} },
		}
	};

	return true;
}

static long
ircd::m::sync::notification_count(const m::room &room,
                    const m::event::idx &a,
                    const m::event::idx &b)
{
    return m::count_since(room, a, a < b? b : a);
}

static long
ircd::m::sync::highlight_count(const m::room &r,
                 const m::user &u,
                 const m::event::idx &a,
                 const m::event::idx &b)
{
    using namespace ircd::m;
    using proto = size_t (const user &, const room &, const event::idx &, const event::idx &);

    static mods::import<proto> count
    {
        "m_user", "highlighted_count__between"
    };

    return count(u, r, a, a < b? b : a);
}
