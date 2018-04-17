// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "sync.int.h"

mapi::header
IRCD_MODULE
{
	"Client 6.2.1 :Sync",
	nullptr,
	on_unload
};

void
on_unload()
{
	synchronizer.interrupt();
	synchronizer.join();
}

//
// sync resource
//

resource
sync_resource
{
	"/_matrix/client/r0/sync",
	{
		sync_description
	}
};

const string_view
sync_description
{R"(

6.2.1

Synchronise the client's state with the latest state on the server. Clients
use this API when they first log in to get an initial snapshot of the state
on the server, and then continue to call this API to get incremental deltas
to the state, and to receive new messages.

)"};

//
// GET sync
//

resource::method
get_sync
{
	sync_resource, "GET", sync,
	{
		get_sync.REQUIRES_AUTH,
		-1s, // TODO: this is only -1 b/c initialsyncs
	}
};

m::import<resource::response (client &, const resource::request &)>
initialsync
{
	"client_initialsync", "initialsync"
};

resource::response
sync(client &client,
     const resource::request &request)
{
	const syncargs args
	{
		request
	};

	return args.since?
		since_sync(client, request, args):
		initialsync(client, request);
}

conf::item<milliseconds>
sync_timeout_max
{
	{ "name",     "ircd.client.sync.timeout.max" },
	{ "default",  305 * 1000L                    },
};

conf::item<milliseconds>
sync_timeout_min
{
	{ "name",     "ircd.client.sync.timeout.min" },
	{ "default",  15 * 1000L                     },
};

conf::item<milliseconds>
sync_timeout_default
{
	{ "name",     "ircd.client.sync.timeout.default" },
	{ "default",  180 * 1000L                        },
};

syncargs::syncargs(const resource::request &request)
:filter_id
{
	// 6.2.1 The ID of a filter created using the filter API or a filter JSON object
	// encoded as a string. The server will detect whether it is an ID or a JSON object
	// by whether the first character is a "{" open brace. Passing the JSON inline is best
	// suited to one off requests. Creating a filter using the filter API is recommended
	// for clients that reuse the same filter multiple times, for example in long poll requests.
	request.query["filter"]
}
,since
{
	// 6.2.1 A point in time to continue a sync from.
	request.query["since"]
}
,timesout{[&request]
{
	// 6.2.1 The maximum time to poll in milliseconds before returning this request.
	auto ret(request.query.get("timeout", milliseconds(sync_timeout_default)));
	ret = std::min(ret, milliseconds(sync_timeout_max));
	ret = std::max(ret, milliseconds(sync_timeout_min));
	return now<steady_point>() + ret;
}()}
,full_state
{
	// 6.2.1 Controls whether to include the full state for all rooms the user is a member of.
	// If this is set to true, then all state events will be returned, even if since is non-empty.
	// The timeline will still be limited by the since parameter. In this case, the timeout
	// parameter will be ignored and the query will return immediately, possibly with an
	// empty timeline. If false, and since is non-empty, only state which has changed since
	// the point indicated by since will be returned. By default, this is false.
	request.query.get("full_state", false)
}
,set_presence
{
	// 6.2.1 Controls whether the client is automatically marked as online by polling this API.
	// If this parameter is omitted then the client is automatically marked as online when it
	// uses this API. Otherwise if the parameter is set to "offline" then the client is not
	// marked as being online when it uses this API. One of: ["offline"]
	request.query.get("set_presence", true)
}
{
}

resource::response
since_sync(client &client,
           const resource::request &request,
           const syncargs &args)
{
	const m::user::room user_room
	{
		request.user_id
	};

	// Can dump pending?
	if(shortpoll_sync(client, request, args))
		return {};

	// Can't dump pending
	return longpoll_sync(client, request, args);
}

bool
shortpoll_sync(client &client,
               const resource::request &request,
               const syncargs &args)

try
{
	const uint64_t _since
	{
		lex_cast<uint64_t>(args.since)
	};

	uint64_t since
	{
		_since
	};

	std::map<std::string, std::vector<std::string>, std::less<>> r;

	m::vm::events::for_each(since, [&]
	(const uint64_t &sequence, const m::event &event)
	{
		if(!r.empty() && (since - _since > 128))
			return false;

		since = sequence;
		if(!json::get<"room_id"_>(event))
			return true;

		const m::room room
		{
			json::get<"room_id"_>(event)
		};

		if(!room.membership(request.user_id))
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
				{ "limited",     false            },
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
			{ "next_batch",  int64_t(since)   },
			{ "rooms",       rooms   },
			{ "presence",    json::object{} },
		}
	};

	return true;
}
catch(const bad_lex_cast &e)
{
	throw m::BAD_REQUEST
	{
		"Since parameter invalid :%s", e.what()
	};

}

resource::response
longpoll_sync(client &client,
              const resource::request &request,
              const syncargs &args)
{
	auto it(begin(polling));
	for(; it != end(polling); ++it)
		if(it->timesout > args.timesout)
			break;

	it = polling.emplace(it, client, request, args);
	add(*it);
	client.longpoll = true;
	synchronizer_dock.notify_one();
	return {};
}

longpoll::longpoll(ircd::client &client,
                   const resource::request &request,
                   const syncargs &args)
:client{weak_from(client)}
,timesout{args.timesout}
,user_id{request.user_id}
,since{args.since}
,access_token{request.access_token}
{
}

//
// Synchronizer worker
//

ircd::context
synchronizer
{
	"synchronizer", 1_MiB, worker, context::POST
};

void
worker()
try
{
	std::unique_lock<decltype(m::vm::accept)> lock
	{
		m::vm::accept
	};

	while(1) try
	{
		auto &accepted
		{
			//m::vm::accept.wait_for(lock, milliseconds(sync_timeout_min))
			m::vm::accept.wait(lock)
		};

		synchronize(accepted);

		// Afterward we reap clients with errors or timed out.
		errored_check();
		timeout_check();
	}
	catch(const timeout &)
	{
		const ctx::exception_handler eh;

		// We land here after sitting in synchronize() for too long without an
		// event being seen. Because clients are polling we have to run a reap.
		timeout_check();
	}
	catch(const ctx::interrupted &)
	{
		throw;
	}
	catch(const std::exception &e)
	{
		log::error
		{
			"Synchronizer worker: %s", e.what()
		};
	}
}
catch(const ctx::interrupted &e)
{
	log::debug
	{
		"Synchronizer worker interrupted"
	};

	return;
}

void
errored_check()
{
	auto it(begin(polling));
	while(it != end(polling))
	{
		auto &sp(*it);
		if(sp.client.expired())
		{
			del(sp);
			it = polling.erase(it);
		}
		else ++it;
	}
}

void
timeout_check()
{
	auto it(begin(polling));
	if(it == end(polling))
		return;

	const auto now{ircd::now<steady_point>()}; do
	{
		auto &sp(*it);
		if(sp.timesout > now)
			return;

		del(sp);
		if(!sp.client.expired())
			timedout(sp.client);

		it = polling.erase(it);
	}
	while(it != end(polling));
}

///
/// TODO: The http error response should not yield this context. If the sendq
/// TODO: is backed up the client should be dc'ed.
bool
timedout(const std::weak_ptr<client> &wp)
try
{
	const life_guard<client> client{wp};
	client->longpoll = false;

	const auto &next_batch
	{
		int64_t(m::vm::current_sequence)
	};

	resource::response
	{
		*client, json::members
		{
			{ "next_batch",  next_batch     },
			{ "rooms",       json::object{} },
			{ "presence",    json::object{} },
		}
	};

	return client->async();
}
catch(const std::bad_weak_ptr &)
{
	return false;
}
catch(const ctx::interrupted &e)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		"synchronizer_timeout(): %s", e.what()
	};

	return false;
}

void
add(longpoll &sp)
{

}

void
del(longpoll &sp)
{

}

void
synchronize(const m::event &event)
{
	const auto &room_id
	{
		json::get<"room_id"_>(event)
	};

	if(room_id)
	{
		synchronize(event, room_id);
		return;
	}
}

void
synchronize(const m::event &event,
            const m::room::id &room_id)
{
	const m::room room
	{
		room_id
	};

	auto it(std::begin(polling));
	while(it != std::end(polling))
	{
		auto &data{*it};
		if(!room.membership(data.user_id))
		{
			++it;
			continue;
		}

		if(!update_sync(data, event, room))
		{
			++it;
			continue;
		}

		it = polling.erase(it);
	}
}

std::string
update_sync_room(client &client,
                 const m::room &room,
                 const string_view &since,
                 const m::event &event)
{
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

	const json::members body
	{
		{ "account_data", json::members{} },
		{ "unread_notifications",
		{
			{ "highlight_count",    int64_t(0) },
			{ "notification_count", int64_t(0) },
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

std::string
update_sync_rooms(client &client,
                  const m::user::id &user_id,
                  const m::room &room,
                  const string_view &since,
                  const m::event &event)
{
	std::vector<std::string> r[3];
	std::vector<json::member> m[3];
	r[0].emplace_back(update_sync_room(client, room, since, event));
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

bool
update_sync(const longpoll &data,
            const m::event &event,
            const m::room &room)
try
{
	const life_guard<client> client
	{
		data.client
	};

	if(!client->longpoll)
		return true;

	const auto rooms
	{
		update_sync_rooms(*client, data.user_id, room, data.since, event)
	};

	const m::user::room ur
	{
		m::user::id{data.user_id}
	};

	std::vector<json::value> presents;
	ur.get(std::nothrow, "m.presence", [&]
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
		*client, json::members
		{
			{ "next_batch",  next_batch  },
			{ "rooms",       rooms       },
			{ "presence",    presence    },
		}
	};

	client->longpoll = false;
	client->async();
	return true;
}
catch(const std::bad_weak_ptr &e)
{
	return true;
}

/*
	if(!defined(json::get<"event_id"_>(event)))
	{
		thread_local char foo[64_KiB];
		mutable_buffer buf{foo};
		m::event event_(event);
		if(at<"type"_>(event) == "m.receipt")
		{
			const auto &content{at<"content"_>(event)};
			for(const auto &room : content)
			{
				const auto &room_id{unquote(room.first)};
				const json::object &object{room.second};
				for(const auto &ev : object)
				{
					const auto &type{unquote(ev.first)};
					const json::object &users{ev.second};
					for(const auto &user : users)
					{
						const auto &user_id{unquote(user.first)};
						const json::object datas{user.second};
						const json::object data{datas.get("data")};
						const auto ts{data.get("ts")};
						const auto event_id{unquote(data.get("event_id"))};
						json::get<"content"_>(event_) = json::stringify(buf, json::members
						{{
							event_id, json::members
							{{
								type, json::members
								{{
									user_id, json::members
									{{
										"ts", ts
									}}
								}}
							}}
						}});
					}
				}
			}
		}

		ephemeral.emplace_back(json::strung(event_));
	}
*/
