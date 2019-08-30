// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::vm::fetch
{
	struct evaltab;

	static void hook_handle_prev(const event &, vm::eval &, evaltab &, const room &);
	static void auth_chain(const room &, const string_view &remote);
	static void hook_handle_auth(const event &, vm::eval &, evaltab &, const room &);
	static void hook_handle(const event &, vm::eval &);

	extern conf::item<seconds> auth_timeout;
	extern conf::item<bool> enable;
	extern hookfn<vm::eval &> hook;
	extern log::log log;
}

struct ircd::m::vm::fetch::evaltab
{
	size_t auth_count {0};
	size_t auth_exists {0};
	size_t prev_count {0};
	size_t prev_exists {0};
	size_t prev_fetching {0};
	size_t prev_fetched {0};
};

ircd::mapi::header
IRCD_MODULE
{
    "Matrix VM Fetch Unit"
};

decltype(ircd::m::vm::fetch::log)
ircd::m::vm::fetch::log
{
	"m.vm.fetch"
};

decltype(ircd::m::vm::fetch::enable)
ircd::m::vm::fetch::enable
{
	{ "name",     "ircd.m.vm.fetch.enable" },
	{ "default",  true                     },
};

decltype(ircd::m::vm::fetch::auth_timeout)
ircd::m::vm::fetch::auth_timeout
{
	{ "name",     "ircd.m.vm.fetch.auth.timeout" },
	{ "default",  15L                            },
};

decltype(ircd::m::vm::fetch::hook)
ircd::m::vm::fetch::hook
{
	hook_handle,
	{
		{ "_site",  "vm.fetch" }
	}
};

//
// fetch_phase
//

void
ircd::m::vm::fetch::hook_handle(const event &event,
                                vm::eval &eval)
try
{
	assert(eval.opts);
	assert(eval.opts->fetch);
	const auto &opts{*eval.opts};

	const auto &type
	{
		at<"type"_>(event)
	};

	if(type == "m.room.create")
		return;

	const m::event::id &event_id
	{
		event.event_id
	};

	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	// Can't construct m::room with the event_id argument because it
	// won't be found (we're evaluating that event here!) so we just set
	// the member manually to make further use of the room struct.
	m::room room{room_id};
	room.event_id = event_id;

	evaltab tab;
	if(opts.fetch_auth_check)
		hook_handle_auth(event, eval, tab, room);

	if(opts.fetch_prev_check)
		hook_handle_prev(event, eval, tab, room);

	log::debug
	{
		log, "%s %s ac:%zu ae:%zu pc:%zu pe:%zu pf:%zu",
		loghead(eval),
		json::get<"room_id"_>(event),
		tab.auth_count,
		tab.auth_exists,
		tab.prev_count,
		tab.prev_exists,
		tab.prev_fetched,
	};
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "%s :%s",
		loghead(eval),
		e.what(),
	};

	throw;
}

void
ircd::m::vm::fetch::hook_handle_auth(const event &event,
                                     vm::eval &eval,
                                     evaltab &tab,
                                     const room &room)

{
	// Count how many of the auth_events provided exist locally.
	const auto &opts{*eval.opts};
	const event::prev prev{event};
	tab.auth_count = prev.auth_events_count();
	for(size_t i(0); i < tab.auth_count; ++i)
	{
		const auto &auth_id
		{
			prev.auth_event(i)
		};

		tab.auth_exists += bool(m::exists(auth_id));
	}

	// We are satisfied at this point if all auth_events for this event exist,
	// as those events have themselves been successfully evaluated and so forth.
	assert(tab.auth_exists <= tab.auth_count);
	if(tab.auth_exists == tab.auth_count)
		return;

	// At this point we are missing one or more auth_events for this event.
	log::dwarning
	{
		log, "%s auth_events:%zu hit:%zu miss:%zu",
		loghead(eval),
		tab.auth_count,
		tab.auth_exists,
		tab.auth_count - tab.auth_exists,
	};

	// We need to figure out where best to sling a request to fetch these
	// missing auth_events. We prefer the remote client conducting this eval
	// with their /federation/send/ request which we stored in the opts.
	const string_view &remote
	{
		opts.node_id?
			opts.node_id:
		!my_host(json::get<"origin"_>(event))?
			string_view(json::get<"origin"_>(event)):
		!my_host(room.room_id.host())?                  //TODO: XXX
			room.room_id.host():
			string_view{}
	};

	// Bail out here if we can't or won't attempt fetching auth_events.
	if(!opts.fetch_auth || !bool(m::vm::fetch::enable) || !remote)
		throw vm::error
		{
			vm::fault::EVENT, "Failed to fetch auth_events for %s in %s",
			string_view{event.event_id},
			json::get<"room_id"_>(event)
		};

	// This is a blocking call to recursively fetch and evaluate the auth_chain
	// for this event. Upon return all of the auth_events for this event will
	// have themselves been fetched and auth'ed recursively or throws.
	auth_chain(room, remote);
	tab.auth_exists = tab.auth_count;
}

void
ircd::m::vm::fetch::auth_chain(const room &room,
                               const string_view &remote)
try
{
	log::debug
	{
		log, "Fetching auth chain for %s in %s (hint: %s)",
		string_view{room.event_id},
		string_view{room.room_id},
		remote,
	};

	m::fetch::opts opts;
	opts.op = m::fetch::op::auth;
	opts.room_id = room.room_id;
	opts.event_id = room.event_id;
	opts.hint = remote;
	auto future
	{
		m::fetch::start(opts)
	};

	const auto result
	{
		future.get(seconds(auth_timeout))
	};

	const json::object response
	{
		result
	};

	const json::array &auth_chain
	{
		response["auth_chain"]
	};

	log::debug
	{
		log, "Evaluating %zu auth events in chain for %s in %s",
		auth_chain.size(),
		string_view{room.event_id},
		string_view{room.room_id},
	};

	m::vm::opts vmopts;
	vmopts.infolog_accept = true;
	vmopts.fetch_prev_check = false;
	vmopts.fetch_state_check = false;
	vmopts.warnlog &= ~vm::fault::EXISTS;
	m::vm::eval
	{
		auth_chain, vmopts
	};
}
catch(const std::exception &e)
{
	thread_local char rembuf[64];
	log::error
	{
		log, "Fetching auth chain for %s in %s from %s :%s",
		string_view{room.event_id},
		string_view{room.room_id},
		string(rembuf, remote),
		e.what(),
	};

	throw;
}

void
ircd::m::vm::fetch::hook_handle_prev(const event &event,
                                     vm::eval &eval,
                                     evaltab &tab,
                                     const room &room)
{
	const auto &opts{*eval.opts};
	const event::prev prev{event};
	tab.prev_count = prev.prev_events_count();
	for(size_t i(0); i < tab.prev_count; ++i)
	{
		const auto &prev_id
		{
			prev.prev_event(i)
		};

		if(m::exists(prev_id))
		{
			++tab.prev_exists;
			continue;
		}

		const bool can_fetch
		{
			opts.fetch_prev && bool(m::vm::fetch::enable)
		};

		const bool fetching
		{
			//TODO: XXX
			can_fetch && false //start(room.room_id, prev_id)
		};

		tab.prev_fetching += fetching;
	}

	// If we have all of the referenced prev_events we are satisfied here.
	assert(tab.prev_exists <= tab.prev_count);
	if(tab.prev_exists == tab.prev_count)
		return;

	// At this point one or more prev_events are missing; the fetches were
	// launched asynchronously if the options allowed for it.
	log::dwarning
	{
		log, "%s prev_events:%zu hit:%zu miss:%zu fetching:%zu",
		loghead(eval),
		tab.prev_count,
		tab.prev_exists,
		tab.prev_count - tab.prev_exists,
		tab.prev_fetching,
	};

	// If the options want to wait for the fetch+evals of the prev_events to occur
	// before we continue processing this event further, we block in here.
	const bool &prev_wait{opts.fetch_prev_wait};
	if(prev_wait && tab.prev_fetching) for(size_t i(0); i < tab.prev_count; ++i)
	{
		const auto &prev_id
		{
			prev.prev_event(i)
		};

		//TODO: XXX
		assert(0);
		tab.prev_fetched += m::exists(prev_id);
	}

	// Aborts this event if the options want us to guarantee at least one
	// prev_event was fetched and evaluated for this event. This is generally
	// used in conjunction with the fetch_prev_wait option to be effective.
	const bool &prev_any{opts.fetch_prev_any};
	if(prev_any && tab.prev_exists + tab.prev_fetched == 0)
		throw vm::error
		{
			vm::fault::EVENT, "Failed to fetch any prev_events for %s in %s",
			string_view{event.event_id},
			json::get<"room_id"_>(event)
		};

	// Aborts this event if the options want us to guarantee ALL of the
	// prev_events were fetched and evaluated for this event.
	const bool &prev_all{opts.fetch_prev_all};
	if(prev_all && tab.prev_exists + tab.prev_fetched < tab.prev_count)
		throw vm::error
		{
			vm::fault::EVENT, "Failed to fetch all %zu required prev_events for %s in %s",
			tab.prev_count,
			string_view{event.event_id},
			json::get<"room_id"_>(event)
		};
}
