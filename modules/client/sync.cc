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

const auto sync_description
{R"(

6.2.1

Synchronise the client's state with the latest state on the server. Clients
use this API when they first log in to get an initial snapshot of the state
on the server, and then continue to call this API to get incremental deltas
to the state, and to receive new messages.

)"};

resource sync_resource
{
	"/_matrix/client/r0/sync",
	{
		sync_description
	}
};

void longpoll(client &client, const resource::request &request, const steady_point &timeout);

void synchronizer_worker();
ircd::context synchronizer_context
{
	"synchronizer",
	256_KiB,
	&synchronizer_worker,
	ircd::context::POST,
};

void synchronizer_timeout_worker();
ircd::context synchronizer_timeout_context
{
	"synchronizer",
	256_KiB,
	&synchronizer_timeout_worker,
	ircd::context::POST,
};

const auto on_unload{[]
{
	synchronizer_context.interrupt();
	synchronizer_timeout_context.interrupt();

	synchronizer_context.join();
	synchronizer_timeout_context.join();
}};

mapi::header
IRCD_MODULE
{
	"Client 6.2.1 :Sync",
	nullptr,
	on_unload
};

resource::response
initial_sync(client &client,
             const resource::request &request,
             const string_view &filter_id,
             const bool &full_state,
             const string_view &set_presence);

resource::response
sync(client &client,
     const resource::request &request)
{
	// 6.2.1 The ID of a filter created using the filter API or a filter JSON object
	// encoded as a string. The server will detect whether it is an ID or a JSON object
	// by whether the first character is a "{" open brace. Passing the JSON inline is best
	// suited to one off requests. Creating a filter using the filter API is recommended
	// for clients that reuse the same filter multiple times, for example in long poll requests.
	const auto filter_id
	{
		request.query["filter"]
	};

	// 6.2.1 A point in time to continue a sync from.
	const auto since
	{
		request.query["since"]
	};

	// 6.2.1 Controls whether to include the full state for all rooms the user is a member of.
	// If this is set to true, then all state events will be returned, even if since is non-empty.
	// The timeline will still be limited by the since parameter. In this case, the timeout
	// parameter will be ignored and the query will return immediately, possibly with an
	// empty timeline. If false, and since is non-empty, only state which has changed since
	// the point indicated by since will be returned. By default, this is false.
	const bool full_state
	{
		request.query["full_state"] == "true"
	};

	// 6.2.1 Controls whether the client is automatically marked as online by polling this API.
	// If this parameter is omitted then the client is automatically marked as online when it
	// uses this API. Otherwise if the parameter is set to "offline" then the client is not
	// marked as being online when it uses this API. One of: ["offline"]
	const string_view set_presence
	{
		request.query["set_presence"]
	};

	// Start a new spool for client
	if(!since)
		return initial_sync(client, request, filter_id, full_state, set_presence);

	// The ircd.tape.head
	int64_t sequence{0};
	//TODO: user's room
	if(!m::user::tokens.get(std::nothrow, "ircd.tape.head"_sv, request.access_token, [&sequence]
	(const m::event &event)
	{
		const json::object &content
		{
			at<"content"_>(event)
		};

		sequence = content.at<int64_t>("sequence");
	}))
		throw m::NOT_FOUND{"since parameter invalid"};

	// 6.2.1 The maximum time to poll in milliseconds before returning this request.
	const int64_t timeout_requested
	{
		request.query["timeout"]? lex_cast<int64_t>(request.query.at("timeout")) : 0L
	};

	const auto timeout_at
	{
		now<steady_point>() +
		milliseconds(timeout_requested? std::max(timeout_requested, 3000L) : 30000L)
	};

	longpoll(client, request, timeout_at);

	// This handler returns no response. As long as this handler doesn't throw
	// an exception IRCd will keep the client alive.
	return {};
}

resource::method get_sync
{
	sync_resource, "GET", sync,
	{
		get_sync.REQUIRES_AUTH
	}
};

/// Input
///
///

struct syncpoll
{
	static std::list<syncpoll> polling;
	static std::multimap<steady_point, decltype(polling)::iterator> pollout;

	std::string user_id;
	std::string since;
	std::string access_token; // can get rid of this and use some session id
	std::weak_ptr<ircd::client> client;
	decltype(pollout)::iterator it { std::end(pollout) };
};

decltype(syncpoll::polling) syncpoll::polling {};
decltype(syncpoll::pollout) syncpoll::pollout {};

void
longpoll(client &client,
         const resource::request &request,
         const steady_point &timeout)
{
	static auto &polling{syncpoll::polling};
	static auto &pollout{syncpoll::pollout};

	const auto it
	{
		polling.emplace(polling.end(), syncpoll
		{
			std::string{request.user_id},
			std::string{request.query.at("since")},
			std::string{request.access_token},       //TODO: nope.
			weak_from(client)
		})
	};

	syncpoll &data
	{
		*it
	};

	data.it = pollout.emplace(timeout, it);

	if(pollout.size() == 1)
		notify(synchronizer_timeout_context);

	client.longpoll = true;
}

// 
// Timeout worker stack
//

void synchronizer_timeout(const syncpoll &sp);

/// This function is the base of an ircd::context which yields until a client
/// is due to timeout. This worker reaps timed out clients from the lists.
void
synchronizer_timeout_worker()
try
{
	static auto &polling{syncpoll::polling};
	static auto &pollout{syncpoll::pollout};

	while(1)
	{
		while(!pollout.empty())
		{
			const auto &timeout{std::begin(pollout)->first};
			const auto &iterator{std::begin(pollout)->second};
			if(timeout > now<steady_point>())
			{
				ctx::wait_until<std::nothrow_t>(timeout);
				continue;
			}

			const auto &data{*iterator};
			synchronizer_timeout(data);
			polling.erase(iterator);
			pollout.erase(std::begin(pollout));
		}

		while(pollout.empty())
			ctx::wait();
	}
}
catch(const ircd::ctx::interrupted &e)
{
	ircd::log::debug("Synchronizer timeout worker interrupted");
}

///
/// TODO: The http error response should not yield this context. If the sendq
/// TODO: is backed up the client should be dc'ed.
void
synchronizer_timeout(const syncpoll &sp)
try
{
	const life_guard<client> client
	{
		sp.client
	};

	resource::response
	{
		*client, http::REQUEST_TIMEOUT
	};

	client->longpoll = false;
	client->async();
}
catch(const std::exception &e)
{
	log::error("synchronizer_timeout(): %s", e.what());
}

//
// Main worker stack
//

bool update_sync(const syncpoll &data, const m::event &event, const m::room &);
void synchronize(const m::event &, const m::room::id &);
void synchronize(const m::event &);

void
synchronizer_worker()
{
	while(1) try
	{
		std::unique_lock<decltype(m::vm::accept)> lock
		{
			m::vm::accept
		};

		// reference to the event on the inserter's stack
		const auto &event
		{
			m::vm::accept.wait(lock)
		};

		if(!syncpoll::polling.empty())
			synchronize(event);
	}
	catch(const ircd::ctx::interrupted &e)
	{
		ircd::log::debug("Synchronizer worker interrupted");
		return;
	}
	catch(const timeout &e)
	{
		ircd::log::debug("Synchronizer worker: %s", e.what());
	}
	catch(const std::exception &e)
	{
		ircd::log::error("Synchronizer worker: %s", e.what());
	}
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

	assert(0);
}

void
synchronize(const m::event &event,
            const m::room::id &room_id)
{
	static auto &polling{syncpoll::polling};
	static auto &pollout{syncpoll::pollout};

	const m::room room
	{
		room_id
	};

	for(auto it(std::begin(polling)); it != std::end(polling);)
	{
		const auto &data{*it};
		if(!room.membership(data.user_id))
		{
			++it;
			continue;
		}

		if(update_sync(data, event, room))
		{
			pollout.erase(data.it);
			polling.erase(it++);
		}
		else ++it;
	}
}

std::string
update_sync_room(client &client,
                 const m::room &room,
                 const string_view &since,
                 const m::event &event)
{
	std::vector<std::string> state;
	if(defined(json::get<"state_key"_>(event)))
		state.emplace_back(json::strung(event));

	const json::strung state_serial
	{
		state.data(), state.data() + state.size()
	};

	std::vector<std::string> timeline;
	if(!defined(json::get<"state_key"_>(event)))
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
		{ "state",
		{
			{ "events", state_serial }
		}},
		{ "timeline",
		{
			{ "events", timeline_serial },
			{ "prev_batch", int64_t(m::vm::current_sequence) },  //TODO: XXX
			{ "limited", false },                       //TODO: XXX
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
update_sync(const syncpoll &data,
            const m::event &event,
            const m::room &room)
try
{
	const life_guard<client> client
	{
		data.client
	};

	const auto rooms
	{
		update_sync_rooms(*client, data.user_id, room, data.since, event)
	};

	const auto presence
	{
		"{}"
	};

	const int64_t &next_batch
	{
		m::vm::current_sequence
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

std::string
initial_sync_room(client &client,
                  const resource::request &request,
                  const m::room &room,
                  const bool &full_state)
{
	std::vector<std::string> state;
	{
		const m::room::state state_
		{
			room
		};

		state_.for_each([&state](const m::event &event)
		{
			state.emplace_back(json::strung(event));
		});
	}

	const auto state_serial
	{
		json::strung(state.data(), state.data() + state.size())
	};

	std::vector<std::string> timeline;
	{
		const m::room::messages timeline_
		{
			room
		};

		timeline_.test([&timeline](const m::event &event)
		{
			timeline.emplace_back(json::strung(event));
			return timeline.size() >= 10;
		});
	}

	const auto timeline_serial
	{
		json::strung(timeline.data(), timeline.data() + timeline.size())
	};

	std::vector<std::string> ephemeral;
	const json::strung ephemeral_serial
	{
		ephemeral.data(), ephemeral.data() + ephemeral.size()
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
		{ "state",
		{
			{ "events", state_serial }
		}},
		{ "timeline",
		{
			{ "events", timeline_serial },
			{ "prev_batch", int64_t(m::vm::current_sequence) },  //TODO: XXX
			{ "limited", false },                       //TODO: XXX
		}},
	};

	return json::strung(body);
}

std::string
initial_sync_rooms(client &client,
                   const resource::request &request,
                   const string_view &filter_id,
                   const bool &full_state)
{
	m::user user{request.user_id};
	const auto user_room_id{user.room_id()};
	m::room::state user_state{user_room_id};

	std::array<std::vector<std::string>, 3> r;
	std::array<std::vector<json::member>, 3> m;

	// Get the rooms the user is a joined member in by iterating the state
	// events in the user's room.
	user_state.for_each("join", [&r, &m, &client, &request, &full_state]
	(const m::event &event)
	{
		const m::room::id &room_id{unquote(at<"state_key"_>(event))};
		const auto i
		{
			//membership == "join"? 0:
			//membership == "leave"? 1:
			//membership == "invite"? 2:
			//-1
			0
		};

		r.at(i).emplace_back(initial_sync_room(client, request, room_id, full_state));
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

resource::response
initial_sync(client &client,
             const resource::request &request,
             const string_view &filter_id,
             const bool &full_state,
             const string_view &set_presence)
{
	const std::string rooms
	{
		initial_sync_rooms(client, request, filter_id, full_state)
	};

	const auto presence
	{
		"{}"
	};

	const int64_t &next_batch
	{
		m::vm::current_sequence
	};

	const auto &state_key
	{
		request.access_token
	};

	//TODO: user's room
	m::send(m::user::tokens, request.user_id, "ircd.tape.head", state_key,
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
