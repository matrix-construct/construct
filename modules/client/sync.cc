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

const auto sync_description
{R"(

6.2.

Synchronise the client's state with the latest state on the server. Clients
use this API when they first log in to get an initial snapshot of the state
on the server, and then continue to call this API to get incremental deltas
to the state, and to receive new messages.

)"};

resource sync_resource
{
	"_matrix/client/r0/sync",
	sync_description
};

struct syncpoll
{
	static std::multimap<std::string, syncpoll> polling;
	static std::multimap<steady_point, decltype(polling)::iterator> pollout;

	std::weak_ptr<ircd::client> client;
	decltype(pollout)::iterator it { std::end(pollout) };
};

decltype(syncpoll::polling) syncpoll::polling {};
decltype(syncpoll::pollout) syncpoll::pollout {};

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

mapi::header IRCD_MODULE
{
	"registers the resource 'client/sync' to handle requests.",
	nullptr,
	on_unload
};

resource::response
sync_now(client &client,
         const resource::request &request,
         const string_view &filter_id,
         const bool &full_state,
         const string_view &set_presence)
{

	json::value events[1];

	const json::members timeline
	{
		{ "events",  { events, 1 } }
	};

	const json::members state
	{
		{ "events",  { events, 1 } }
	};

	const json::members join
	{
		{ "timeline",   timeline },
		{ "state",      state    },
	};

	const json::object leave{};
	const json::object invite{};
	const json::members rooms
	{
		{ "leave",   leave  },
		{ "join",    join   },
		{ "invite",  invite },
	};

	const string_view next_batch{};
	const json::object presence{};

	const m::event::id head_event_id
	{
		"$12382382:cdc.z"
	};

	const json::members content
	{
		{ "event_id",  head_event_id }
	};

	m::user::sessions.send(
	{
		{ "type",       "ircd.tape.head"                 },
		{ "state_key",  request.query.at("access_token") },
		{ "sender",     request.user_id                  },
		{ "content",    content                          },
	});

	return resource::response
	{
		client, json::members
		{
			{ "next_batch",  next_batch  },
			{ "rooms",       rooms       },
			{ "presence",    presence    }
		}
	};
}

resource::response
sync(client &client, const resource::request &request)
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

	if(!since)
		return sync_now(client, request, filter_id, full_state, set_presence);

	// The !sessions:your.host room is where the ircd.tape.head event holds
	// the state we use to calculate the last event the user has seen.
	const m::room::state sessions
	{
		m::user::sessions
	};

	// The ircd.tape.head
	const m::event::where::equal query
	{
		{ "type",       "ircd.tape.head"                 },
		{ "state_key",  request.query.at("access_token") },
	};

	m::event::id::buf head;
	if(!sessions.test(query, [&head](const auto &event)
	{
		const json::object &content
		{
			at<m::name::content>(event)
		};

		head = unquote(content.at("event_id"));
		return true;
	}))
		throw m::NOT_FOUND{"since parameter invalid"};


	// 6.2.1 The maximum time to poll in milliseconds before returning this request.
	const auto timeout
	{
		request.query["timeout"]
	};

	const auto timeout_at
	{
		now<steady_point>() + milliseconds(std::max(timeout? lex_cast<int64_t>(timeout) : 30000L, 1000L))
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
void
longpoll(client &client,
         const resource::request &request,
         const steady_point &timeout)
{
	const auto it
	{
		syncpoll::polling.emplace(request.user_id, syncpoll{weak_from(client)})
	};

	syncpoll &data
	{
		it->second
	};

	data.it = syncpoll::pollout.emplace(timeout, it);

	if(syncpoll::pollout.size() == 1)
		notify(synchronizer_timeout_context);
}

// 
// Timeout worker stack
//

void synchronizer_timeout(const std::string &user_id, const syncpoll &sp);

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

			const auto &user_id{iterator->first};
			const auto &data{iterator->second};
			synchronizer_timeout(user_id, data);
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
synchronizer_timeout(const std::string &user_id,
                     const syncpoll &sp)
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
}
catch(const std::exception &e)
{
	log::error("synchronizer_timeout(): %s", e.what());
}

//
// Main worker stack
//

void synchronize(const m::event &, const m::room::id &);
void synchronize(const m::event &);

void
synchronizer_worker()
try
{
	while(1) try
	{
		const auto &event
		{
			m::event::inserted.wait()
		};

		synchronize(event);
	}
	catch(const timeout &e)
	{
		ircd::log::debug("Synchronizer worker timeout");
	}
}
catch(const ircd::ctx::interrupted &e)
{
	ircd::log::debug("Synchronizer worker interrupted");
}

void
synchronize(const m::event &event)
{
	static auto &polling{syncpoll::polling};
	static auto &pollout{syncpoll::pollout};

	const auto &room_id
	{
		json::val<m::name::room_id>(event)
	};

	if(room_id)
	{
		synchronize(event, room_id);
		return;
	}

	std::cout << event << std::endl;
}

void
synchronize(const m::event &event,
            const m::room::id &room_id)
{
	std::cout << event << std::endl;
}

bool
handle_event(const m::event &event,
             const syncpoll &request)
try
{
	const life_guard<const client> client
	{
		request.client
	};

//	if(request.timeout < now<steady_point>())
//		return false;

	return true;
}
catch(const std::exception &e)
{
	log::error("%s", e.what());
	return false;
}
