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

decltype(ircd::m::sync::log)
ircd::m::sync::log
{
	"sync", 's'
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

ircd::conf::item<size_t>
ircd::m::sync::shortpoll::flush_hiwat
{
	{ "name",     "ircd.client.sync.flush.hiwat"  },
	{ "default",  long(24_KiB)                    },
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

	shortpoll sp
	{
		client, args
	};

	if(sp.since > sp.current)
		throw m::NOT_FOUND
		{
			"Since parameter is in the future..."
		};

	json::stack::object top
	{
		sp.out
	};

	const size_t linear_delta_max
	{
		linear::delta_max
	};

	const bool shortpolled
	{
		sp.delta == 0?
			false:
		sp.delta > linear_delta_max?
			polylog::handle(client, sp, top):
			linear::handle(client, sp, top)
	};

	// When shortpoll was successful, do nothing else.
	if(shortpolled)
		return {};

	// When longpoll was successful, do nothing else.
	if(longpoll::poll(client, args))
		return {};

	// A user-timeout occurred. According to the spec we return a
	// 200 with empty fields rather than a 408.
	const json::value next_batch
	{
		lex_cast(m::vm::current_sequence), json::STRING
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

		if(handle(client, args, a))
			return true;
	}
}

bool
ircd::m::sync::longpoll::handle(client &client,
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
		return handle(client, args, event, room);
	}

	return false;
}

bool
ircd::m::sync::longpoll::handle(client &client,
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
		int64_t(m::vm::current_sequence)
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
ircd::m::sync::linear::handle(client &client,
                              shortpoll &sp,
                              json::stack::object &object)
{
	uint64_t since
	{
		sp.since
	};

	std::map<std::string, std::vector<std::string>, std::less<>> r;

	bool limited{false};
	m::events::for_each(since, [&]
	(const uint64_t &sequence, const m::event &event)
	{
		if(!r.empty() && (since - sp.since > 128))
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

		if(!room.membership(sp.args.request.user_id))
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
			m::receipt::read(last_read_buf, room_id, sp.user)
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
				notification_count(room_id, last_read_idx, sp.current):
				json::undefined_number
		};

		const auto highlights
		{
			last_read_idx?
				highlight_count(room_id, sp.user, last_read_idx, sp.current):
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
		const auto membership{m::room{room_id}.membership(membership_buf, sp.user)};
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
			{ "next_batch",  json::value { lex_cast(int64_t(since)), json::STRING } },
			{ "rooms",       rooms   },
			{ "presence",    json::object{} },
		}
	};

	return true;
}

//
// polylog
//

ircd::conf::item<bool>
ircd::m::sync::polylog::prefetch_state
{
	{ "name",     "ircd.client.sync.polylog.prefetch.state"  },
	{ "default",  true                                       },
};

ircd::conf::item<bool>
ircd::m::sync::polylog::prefetch_timeline
{
	{ "name",     "ircd.client.sync.polylog.prefetch.timeline"  },
	{ "default",  true                                          },
};

bool
ircd::m::sync::polylog::handle(client &client,
                               shortpoll &sp,
                               json::stack::object &object)
try
{
	// Generate individual stats for sections
	thread_local char iecbuf[64], rembuf[128];
	sync::stats stats{sp.stats};
	stats.timer = timer{};

	{
		json::stack::member member{object, "rooms"};
		json::stack::object object{member};
		rooms(sp, object);
	}

	#ifdef RB_DEBUG
	log::debug
	{
		log, "polylog %s %s rooms %s wc:%zu in %lu$ms",
		string(rembuf, ircd::remote(sp.client)),
		string_view{sp.request.user_id},
		pretty(iecbuf, iec(sp.stats.flush_bytes - stats.flush_bytes)),
		sp.stats.flush_count - stats.flush_count,
		stats.timer.at<milliseconds>().count()
	};
	stats = sync::stats{sp.stats};
	stats.timer = timer{};
	#endif

	{
		json::stack::member member{object, "presence"};
		json::stack::object object{member};
		presence(sp, object);
	}

	#ifdef RB_DEBUG
	log::debug
	{
		log, "polylog %s %s presence %s wc:%zu in %lu$ms",
		string(rembuf, ircd::remote(sp.client)),
		string_view{sp.request.user_id},
		pretty(iecbuf, iec(sp.stats.flush_bytes - stats.flush_bytes)),
		sp.stats.flush_count - stats.flush_count,
		stats.timer.at<milliseconds>().count()
	};
	stats = sync::stats{sp.stats};
	stats.timer = timer{};
	#endif

	{
		json::stack::member member{object, "account_data"};
		json::stack::object object{member};
		account_data(sp, object);
	}

	#ifdef RB_DEBUG
	log::debug
	{
		log, "polylog %s %s account_data %s wc:%zu in %lu$ms",
		string(rembuf, ircd::remote(sp.client)),
		string_view{sp.request.user_id},
		pretty(iecbuf, iec(sp.stats.flush_bytes - stats.flush_bytes)),
		sp.stats.flush_count - stats.flush_count,
		stats.timer.at<milliseconds>().count()
	};
	#endif

	{
		json::stack::member member
		{
			object, "next_batch", json::value(lex_cast(int64_t(sp.current)), json::STRING)
		};
	}

	log::info
	{
		log, "polylog %s %s %s wc:%zu in %lu$ms",
		string(rembuf, ircd::remote(sp.client)),
		string_view{sp.request.user_id},
		pretty(iecbuf, iec(sp.stats.flush_bytes)),
		sp.stats.flush_count,
		sp.stats.timer.at<milliseconds>().count()
	};

	return sp.committed;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "polylog sync FAILED %lu to %lu (vm @ %zu) :%s"
		,sp.since
		,sp.current
		,m::vm::current_sequence
		,e.what()
	};

	throw;
}

void
ircd::m::sync::polylog::presence(shortpoll &sp,
                                 json::stack::object &out)
{
	json::stack::member member{out, "events"};
	json::stack::array array{member};

	const m::user::mitsein mitsein
	{
		sp.user
	};

	mitsein.for_each("join", [&sp, &array]
	(const m::user &user)
	{
		const m::user::room user_room{user};
		if(head_idx(std::nothrow, user_room) <= sp.since)
			return;

		//TODO: can't check event_idx cuz only closed presence content
		m::presence::get(std::nothrow, user, [&sp, &array]
		(const json::object &event)
		{
			json::stack::object object{array};

			// sender
			{
				json::stack::member member
				{
					object, "sender", unquote(event.get("user_id"))
				};
			}

			// type
			{
				json::stack::member member
				{
					object, "type", json::value{"m.presence"}
				};
			}

			// content
			{
				json::stack::member member
				{
					object, "content", event
				};
			}
		});
	});
}

void
ircd::m::sync::polylog::account_data(shortpoll &sp,
                                     json::stack::object &out)
try
{
	json::stack::member member{out, "events"};
	json::stack::array array{member};
	const m::room::state state
	{
		sp.user_room
	};

	state.for_each("ircd.account_data", [&sp, &array]
	(const m::event &event)
	{
		const auto &event_idx
		{
			index(event, std::nothrow)
		};

		if(event_idx < sp.since || event_idx >= sp.current)
			return;

		json::stack::object object{array};

		// type
		{
			json::stack::member member
			{
				object, "type", at<"state_key"_>(event)
			};
		}

		// content
		{
			json::stack::member member
			{
				object, "content", at<"content"_>(event)
			};
		}
	});
}
catch(const json::not_found &e)
{
	log::critical
	{
		log, "polylog sync account data error %lu to %lu (vm @ %zu) :%s"
		,sp.since
		,sp.current
		,m::vm::current_sequence
		,e.what()
	};
}

void
ircd::m::sync::polylog::rooms(shortpoll &sp,
                              json::stack::object &object)
{
	sync_rooms(sp, object, "invite");
	sync_rooms(sp, object, "join");
	sync_rooms(sp, object, "leave");
	sync_rooms(sp, object, "ban");
}

void
ircd::m::sync::polylog::sync_rooms(shortpoll &sp,
                                   json::stack::object &out,
                                   const string_view &membership)
{
	json::stack::member rooms_member{out, membership};
	json::stack::object rooms_object{rooms_member};
	sp.rooms.for_each(membership, [&sp, &rooms_object]
	(const m::room &room, const string_view &membership)
	{
		if(head_idx(std::nothrow, room) <= sp.since)
			return;

		// Generate individual stats for this room's sync
		#ifdef RB_DEBUG
		sync::stats stats{sp.stats};
		stats.timer = timer{};
		#endif

		// This scope ensures the object destructs and flushes before
		// the log message tallying the stats for this room below.
		{
			json::stack::member member{rooms_object, room.room_id};
			json::stack::object object{member};
			sync_room(sp, object, room, membership);
		}

		#ifdef RB_DEBUG
		thread_local char iecbuf[64], rembuf[128];
		log::debug
		{
			log, "polylog %s %s %s %s wc:%zu in %lu$ms",
			string(rembuf, ircd::remote(sp.client)),
			string_view{sp.request.user_id},
			string_view{room.room_id},
			pretty(iecbuf, iec(sp.stats.flush_bytes - stats.flush_bytes)),
			sp.stats.flush_count - stats.flush_count,
			stats.timer.at<milliseconds>().count()
		};
		#endif
	});
}

void
ircd::m::sync::polylog::sync_room(shortpoll &sp,
                                  json::stack::object &out,
                                  const m::room &room,
                                  const string_view &membership)
try
{
	// timeline
	{
		json::stack::member member{out, "timeline"};
		json::stack::object object{member};
		room_timeline(sp, object, room);
	}

	// state
	{
		json::stack::member member
		{
			out, membership != "invite"?
				"state":
				"invite_state"
		};

		json::stack::object object{member};
		room_state(sp, object, room);
	}

	// ephemeral
	{
		json::stack::member member{out, "ephemeral"};
		json::stack::object object{member};
		room_ephemeral(sp, object, room);
	}

	// account_data
	{
		json::stack::member member{out, "account_data"};
		json::stack::object object{member};
		room_account_data(sp, object, room);
	}

	// unread_notifications
	{
		json::stack::member member{out, "unread_notifications"};
		json::stack::object object{member};
		room_unread_notifications(sp, object, room);
	}
}
catch(const json::not_found &e)
{
	log::critical
	{
		log, "polylog sync room %s error %lu to %lu (vm @ %zu) :%s"
		,string_view{room.room_id}
		,sp.since
		,sp.current
		,m::vm::current_sequence
		,e.what()
	};
}

void
ircd::m::sync::polylog::room_state(shortpoll &sp,
                                   json::stack::object &out,
                                   const m::room &room)
{
	static const m::event::fetch::opts fopts
	{
		m::event::keys::include
		{
			"content",
			"depth",
			"event_id",
			"origin_server_ts",
			"redacts",
			"room_id",
			"sender",
			"state_key",
			"type",
		},
	};

	json::stack::member member
	{
		out, "events"
	};

	json::stack::array array
	{
		member
	};

	m::room::state state
	{
		room
	};

	if(bool(prefetch_state))
		state.prefetch(sp.since, sp.current);

	state.for_each([&sp, &array]
	(const m::event::idx &event_idx)
	{
		if(event_idx < sp.since || event_idx >= sp.current)
			return;

		const event::fetch event
		{
			event_idx, std::nothrow, &fopts
		};

		if(!event.valid || at<"depth"_>(event) >= int64_t(sp.state_at))
			return;

		array.append(event);
		sp.committed = true;
	});
}

void
ircd::m::sync::polylog::room_timeline(shortpoll &sp,
                                      json::stack::object &out,
                                      const m::room &room)
{
	// events
	bool limited{false};
	m::event::id::buf prev;
	{
		json::stack::member member{out, "events"};
		json::stack::array array{member};
		prev = room_timeline_events(sp, array, room, limited);
	}

	// prev_batch
	{
		json::stack::member member
		{
			out, "prev_batch", string_view{prev}
		};
	}

	// limited
	{
		json::stack::member member
		{
			out, "limited", json::value{limited}
		};
	}
}

ircd::m::event::id::buf
ircd::m::sync::polylog::room_timeline_events(shortpoll &sp,
                                             json::stack::array &out,
                                             const m::room &room,
                                             bool &limited)
{
	static const m::event::fetch::opts fopts
	{
		m::event::keys::include
		{
			"content",
			"depth",
			"event_id",
			"origin_server_ts",
			"prev_events",
			"redacts",
			"room_id",
			"sender",
			"state_key",
			"type",
		},
	};

	// messages seeks to the newest event, but the client wants the oldest
	// event first so we seek down first and then iterate back up. Due to
	// an issue with rocksdb's prefix-iteration this iterator becomes
	// toxic as soon as it becomes invalid. As a result we have to copy the
	// event_id on the way down in case of renewing the iterator for the
	// way back. This is not a big deal but rocksdb should fix their shit.
	ssize_t i(0);
	m::event::id::buf event_id;
	m::room::messages it
	{
		room, &fopts
	};

	for(; it && i < 10; --it, ++i)
	{
		event_id = it.event_id();
		if(it.event_idx() < sp.since)
			break;

		if(it.event_idx() >= sp.current)
			break;

		if(bool(prefetch_timeline))
			m::prefetch(it.event_idx(), fopts);
	}

	limited = i >= 10;
	sp.committed |= i > 0;

	if(i > 0 && !it)
		it.seek(event_id);

	if(i > 0 && it)
	{
		const m::event &event{*it};
		sp.state_at = at<"depth"_>(event);
	}

	if(i > 0)
		for(; it && i > -1; ++it, --i)
			out.append(*it);

	return event_id;
}

void
ircd::m::sync::polylog::room_ephemeral(shortpoll &sp,
                                      json::stack::object &out,
                                      const m::room &room)
{
	{
		json::stack::member member{out, "events"};
		json::stack::array array{member};
		room_ephemeral_events(sp, array, room);
	}
}

void
ircd::m::sync::polylog::room_ephemeral_events(shortpoll &sp,
                                              json::stack::array &out,
                                              const m::room &room)
{
	const m::room::members members{room};
	members.for_each("join", m::room::members::closure{[&]
	(const m::user &user)
	{
		static const m::event::fetch::opts fopts
		{
			m::event::keys::include
			{
				"event_id",
				"content",
				"sender",
			},
		};

		m::user::room user_room{user};
		user_room.fopts = &fopts;

		if(head_idx(std::nothrow, user_room) <= sp.since)
			return;

		user_room.get(std::nothrow, "ircd.read", room.room_id, [&]
		(const m::event &event)
		{
			const auto &event_idx
			{
				index(event, std::nothrow)
			};

			if(event_idx < sp.since || event_idx >= sp.current)
				return;

			sp.committed = true;
			json::stack::object object{out};

			// type
			{
				json::stack::member member
				{
					object, "type", "m.receipt"
				};
			}

			// content
			{
				const json::object data
				{
					at<"content"_>(event)
				};

				thread_local char buf[1024];
				const json::members reformat
				{
					{ unquote(data.at("event_id")),
					{
						{ "m.read",
						{
							{ at<"sender"_>(event),
							{
								{ "ts", data.at("ts") }
							}}
						}}
					}}
				};

				json::stack::member member
				{
					object, "content", json::stringify(mutable_buffer{buf}, reformat)
				};
			}
		});
	}});
}

void
ircd::m::sync::polylog::room_account_data(shortpoll &sp,
                                          json::stack::object &out,
                                          const m::room &room)
{
	json::stack::member member{out, "events"};
	json::stack::array array{member};
	const m::room::state state
	{
		sp.user_room
	};

	char typebuf[288]; //TODO: room_account_data_typebuf_size
	const auto type
	{
		m::user::_account_data_type(typebuf, room.room_id)
	};

	state.for_each(type, [&sp, &array]
	(const m::event &event)
	{
		const auto &event_idx
		{
			index(event, std::nothrow)
		};

		if(event_idx < sp.since || event_idx >= sp.current)
			return;

		json::stack::object object{array};

		// type
		{
			json::stack::member member
			{
				object, "type", at<"state_key"_>(event)
			};
		}

		// content
		{
			json::stack::member member
			{
				object, "content", at<"content"_>(event)
			};
		}
	});
}

void
ircd::m::sync::polylog::room_unread_notifications(shortpoll &sp,
                                                  json::stack::object &out,
                                                  const m::room &room)
{
	m::event::id::buf last_read;
	if(!m::receipt::read(last_read, room.room_id, sp.user))
		return;

	const auto last_read_idx
	{
		index(last_read)
	};

	// highlight_count
	json::stack::member
	{
		out, "highlight_count", json::value
		{
			highlight_count(room, sp.user, last_read_idx, sp.current)
		}
	};

	// notification_count
	json::stack::member
	{
		out, "notification_count", json::value
		{
			notification_count(room, last_read_idx, sp.current)
		}
	};
}

long
ircd::m::sync::highlight_count(const room &r,
                               const user &u,
                               const event::idx &a,
                               const event::idx &b)
{
	using proto = size_t (const user &, const room &, const event::idx &, const event::idx &);

	static mods::import<proto> count
	{
		"m_user", "highlighted_count__between"
	};

	return count(u, r, a, a < b? b : a);
}

long
ircd::m::sync::notification_count(const room &room,
                                  const event::idx &a,
                                  const event::idx &b)
{
	return m::count_since(room, a, a < b? b : a);
}
