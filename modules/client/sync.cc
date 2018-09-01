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

	static const auto max_linear_sync
	{
		384 //TODO: conf
	};

	const bool shortpolled
	{
		sp.delta == 0?
			false:
		sp.delta > max_linear_sync?
			polylog::handle(client, sp, top):
			linear::handle(client, sp, top)
	};

	if(!shortpolled)
		longpoll::poll(client, args);

	return {};
}
catch(const bad_lex_cast &e)
{
	throw m::BAD_REQUEST
	{
		"Since parameter invalid :%s", e.what()
	};
}

void
ircd::m::sync::longpoll::poll(client &client,
                              const args &args)
try
{
	std::unique_lock<decltype(m::vm::accept)> lock
	{
		m::vm::accept
	};

	while(1)
	{
		auto &accepted
		{
			m::vm::accept.wait_until(lock, args.timesout)
		};

		assert(accepted.opts);
		if(!accepted.opts->notify_clients)
			continue;

		if(handle(client, args, accepted))
			return;
	}
}
catch(const ctx::timeout &e)
{
	const ctx::exception_handler eh;

	const int64_t &since
	{
		int64_t(m::vm::current_sequence)
	};

	resource::response
	{
		client, json::members
		{
			{ "next_batch",  json::value { lex_cast(int64_t(since)), json::STRING }  },
			{ "rooms",       json::object{}  },
			{ "presence",    json::object{}  },
		}
	};
}

bool
ircd::m::sync::longpoll::handle(client &client,
                                const args &args,
                                const m::event &event)
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
                                const m::event &event,
                                const m::room &room)
{
	const m::user::id &user_id
	{
		args.request.user_id
	};

	if(!room.membership(user_id, "join"))
		return false;

//	if(json::get<"type"_>(event) == "m.room.message")
//		if(json::get<"sender"_>(event) == user_id)
//			return false;


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
                                    const m::event &event)
{
	std::vector<std::string> r[3];
	std::vector<json::member> m[3];
	r[0].emplace_back(sync_room(client, room, args, event));
	m[0].emplace_back(room.room_id, r[0].back());

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
ircd::m::sync::longpoll::sync_room(client &client,
                                   const m::room &room,
                                   const args &args,
                                   const m::event &event)
{
	const auto &since
	{
		args.since
	};

	std::vector<std::string> state;
	if(defined(json::get<"event_id"_>(event)) && defined(json::get<"state_key"_>(event)))
		state.emplace_back(json::strung(event));

	const json::strung state_serial
	{
		state.data(), state.data() + state.size()
	};

	std::vector<std::string> timeline;
	if(defined(json::get<"event_id"_>(event)) && !defined(json::get<"state_key"_>(event)))
		timeline.emplace_back(json::strung(event));

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

	const int64_t notes
	{
		0
	};

	const json::members body
	{
		{ "account_data", json::members{} },
		{ "unread_notifications",
		{
			{ "highlight_count",    int64_t(0) },
			{ "notification_count", notes },
		}},
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
	};

	return json::strung(body);
}

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

	std::vector<json::member> joins;

	for(auto &p : r)
	{
		const auto &room_id{p.first};
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
		};

		joins.emplace_back(room_id, body);
	};

	const json::value join
	{
		joins.data(), joins.size()
	};

	const json::members rooms
	{
		{ "join",   join           },
		{ "leave",  json::object{} },
		{ "invite", json::object{} },
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

bool
ircd::m::sync::polylog::handle(client &client,
                               shortpoll &sp,
                               json::stack::object &object)
try
{
	{
		json::stack::member member{object, "rooms"};
		json::stack::object object{member};
		rooms(sp, object);
	}

	{
		json::stack::member member{object, "presence"};
		json::stack::object object{member};
		presence(sp, object);
	}

	{
		json::stack::member member{object, "account_data"};
		json::stack::object object{member};
		account_data(sp, object);
	}

	{
		json::stack::member member
		{
			object, "next_batch", json::value(lex_cast(int64_t(sp.current)), json::STRING)
		};
	}

	std::cout << "polylog sync in: " << sp.stats.timer.at<seconds>().count() << " seconds" << std::endl;

	return sp.committed;
}
catch(const std::exception &e)
{
	log::error
	{
		"polylog sync FAILED %lu to %lu (vm @ %zu) :%s"
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

try
{

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
catch(const json::not_found &e)
{
	log::error
	{
		"polylog sync for presence error %lu to %lu (vm @ %zu) :%s"
		,sp.since
		,sp.current
		,m::vm::current_sequence
		,e.what()
	};
}

}

void
ircd::m::sync::polylog::account_data(shortpoll &sp,
                                     json::stack::object &out)
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

void
ircd::m::sync::polylog::rooms(shortpoll &sp,
                              json::stack::object &object)
{
	{
		json::stack::member member{object, "join"};
		json::stack::object object{member};
		rooms__membership(sp, object, "join");
	}

	{
		json::stack::member member{object, "leave"};
		json::stack::object object{member};
		rooms__membership(sp, object, "leave");
	}

	{
		json::stack::member member{object, "invite"};
		json::stack::object object{member};
		rooms__membership(sp, object, "invite");
	}
}

void
ircd::m::sync::polylog::rooms__membership(shortpoll &sp,
                                          json::stack::object &object,
                                          const string_view &membership)
{
	sp.rooms.for_each(membership, [&]
	(const m::room &room, const string_view &)
	{
		if(head_idx(std::nothrow, room) <= sp.since)
			return;

		const m::room::id &room_id{room.room_id};
		json::stack::member member{object, room_id};
		json::stack::object object{member};
		std::cout << "sync: " << room_id << std::endl;
		sync_room(sp, object, room, membership);
		std::cout << "done: " << room_id << std::endl;
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
	log::error
	{
		"polylog sync for room %s error %lu to %lu (vm @ %zu) :%s"
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
			"membership",
			"origin_server_ts",
			//"prev_events",
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
		room, &fopts
	};

	state.for_each([&]
	(const m::event &event)
	{
		if(at<"depth"_>(event) >= int64_t(sp.state_at))
			return;

		const auto &event_idx
		{
			index(event, std::nothrow)
		};

		if(event_idx < sp.since || event_idx >= sp.current)
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
			"membership",
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
	// highlight_count
	{
		json::stack::member member
		{
			out, "highlight_count", json::value{0L}
		};
	}

	// notification_count
	{
		json::stack::member member
		{
			out, "notification_count", json::value{0L}
		};
	}
}
