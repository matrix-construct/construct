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
	static void prev_check(const event &, vm::eval &);
	static bool prev_wait(const event &, vm::eval &);
	static std::forward_list<ctx::future<m::fetch::result>> prev_fetch(const event &, vm::eval &, const room &);
	static void prev(const event &, vm::eval &, const room &);
	static std::forward_list<ctx::future<m::fetch::result>> state_fetch(const event &, vm::eval &, const room &);
	static void state(const event &, vm::eval &, const room &);
	static void auth_chain_eval(const event &, vm::eval &, const room &, const json::array &);
	static void auth_chain(const event &, vm::eval &, const room &);
	static void auth(const event &, vm::eval &, const room &);
	static void handle(const event &, vm::eval &);

	extern conf::item<milliseconds> prev_fetch_check_interval;
	extern conf::item<milliseconds> prev_wait_time;
	extern conf::item<size_t> prev_wait_count;
	extern conf::item<size_t> prev_backfill_limit;
	extern conf::item<seconds> event_timeout;
	extern conf::item<seconds> state_timeout;
	extern conf::item<seconds> auth_timeout;
	extern conf::item<bool> enable;
	extern hookfn<vm::eval &> hook;
	extern log::log log;
}

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

decltype(ircd::m::vm::fetch::hook)
ircd::m::vm::fetch::hook
{
	handle,
	{
		{ "_site",  "vm.fetch" }
	}
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

decltype(ircd::m::vm::fetch::state_timeout)
ircd::m::vm::fetch::state_timeout
{
	{ "name",     "ircd.m.vm.fetch.state.timeout" },
	{ "default",  20L                             },
};

decltype(ircd::m::vm::fetch::event_timeout)
ircd::m::vm::fetch::event_timeout
{
	{ "name",     "ircd.m.vm.fetch.event.timeout" },
	{ "default",  10L                             },
};

decltype(ircd::m::vm::fetch::prev_backfill_limit)
ircd::m::vm::fetch::prev_backfill_limit
{
	{ "name",     "ircd.m.vm.fetch.prev.backfill.limit" },
	{ "default",  128L                                  },
};

decltype(ircd::m::vm::fetch::prev_wait_count)
ircd::m::vm::fetch::prev_wait_count
{
	{ "name",     "ircd.m.vm.fetch.prev.wait.count" },
	{ "default",  4L                                },
};

decltype(ircd::m::vm::fetch::prev_wait_time)
ircd::m::vm::fetch::prev_wait_time
{
	{ "name",     "ircd.m.vm.fetch.prev.wait.time" },
	{ "default",  200L                             },
};

decltype(ircd::m::vm::fetch::prev_fetch_check_interval)
ircd::m::vm::fetch::prev_fetch_check_interval
{
	{ "name",     "ircd.m.vm.fetch.prev.fetch.check_interval" },
	{ "default",  500L                                        },
};

//
// fetch_phase
//

void
ircd::m::vm::fetch::handle(const event &event,
                           vm::eval &eval)
try
{
	assert(eval.opts);
	assert(eval.opts->phase[phase::FETCH]);
	const auto &opts
	{
		*eval.opts
	};

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

	if(opts.fetch_auth)
		auth(event, eval, room);

	if(opts.fetch_prev)
		prev(event, eval, room);

	if(opts.fetch_state)
		state(event, eval, room);
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

//
// auth_events handler stack
//

void
ircd::m::vm::fetch::auth(const event &event,
                         vm::eval &eval,
                         const room &room)

try
{
	// Count how many of the auth_events provided exist locally.
	const auto &opts{*eval.opts};
	const event::prev prev{event};
	const size_t auth_count
	{
		prev.auth_events_count()
	};

	size_t auth_exists{0};
	for(size_t i(0); i < auth_count; ++i)
	{
		const auto &auth_id
		{
			prev.auth_event(i)
		};

		auth_exists += bool(m::exists(auth_id));
	}

	// We are satisfied at this point if all auth_events for this event exist,
	// as those events have themselves been successfully evaluated and so forth.
	assert(auth_exists <= auth_count);
	if(auth_exists == auth_count)
		return;

	// At this point we are missing one or more auth_events for this event.
	log::dwarning
	{
		log, "%s auth_events:%zu miss:%zu",
		loghead(eval),
		auth_count,
		auth_count - auth_exists,
	};

	if(!bool(m::vm::fetch::enable))
		throw vm::error
		{
			vm::fault::AUTH, "Fetching auth_events disabled by configuration",
		};

	if(!opts.fetch_auth)
		throw vm::error
		{
			vm::fault::AUTH, "Not fetching auth_events for this evaluation",
		};

	// This is a blocking call to recursively fetch and evaluate the auth_chain
	// for this event. Upon return all of the auth_events for this event will
	// have themselves been fetched and auth'ed recursively.
	auth_chain(event, eval, room);
}
catch(const std::exception &e)
{
	throw vm::error
	{
		vm::fault::AUTH, "Failed to fetch all auth_events :%s",
		string_view{event.event_id},
		json::get<"room_id"_>(event),
		e.what()
	};
}

void
ircd::m::vm::fetch::auth_chain(const event &event,
                               vm::eval &eval,
                               const room &room)
try
{
	assert(eval.opts);
	m::fetch::opts opts;
	opts.op = m::fetch::op::auth;
	opts.room_id = room.room_id;
	opts.event_id = room.event_id;

	// Figure out a remote hint as the primary target to request the missing
	// auth events from; if provided, m::fetch will ask this remote first. We
	// try to use the eval.node_id, which is set to a server that is conducting
	// the eval (i.e in a /send/ or when processing some response data from
	// them); next we try the origin of the event itself. These remotes are
	// most likely to provide a satisfying response.
	opts.hint =
	{
		eval.opts->node_id?
			eval.opts->node_id:
		!my_host(json::get<"origin"_>(event))?
			string_view(json::get<"origin"_>(event)):
			string_view{}
	};

	log::debug
	{
		log, "Fetching auth chain for %s in %s hint:%s",
		string_view{room.event_id},
		string_view{room.room_id},
		opts.hint,
	};

	// send
	auto future
	{
		m::fetch::start(opts)
	};

	// recv
	const auto result
	{
		future.get(seconds(auth_timeout))
	};

	const json::object response
	{
		result
	};

	// parse
	const json::array &auth_chain
	{
		response["auth_chain"]
	};

	auth_chain_eval(event, eval, room, auth_chain);
}
catch(const vm::error &e)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Fetching auth chain for %s in %s :%s",
		string_view{room.event_id},
		string_view{room.room_id},
		e.what(),
	};

	throw;
}

void
ircd::m::vm::fetch::auth_chain_eval(const event &event,
                                    vm::eval &eval,
                                    const room &room,
                                    const json::array &auth_chain)
try
{
	assert(eval.opts);
	auto opts(*eval.opts);
	opts.fetch_prev = false;
	opts.fetch_state = false;
	opts.fetch_auth = false;
	opts.infolog_accept = true;
	opts.warnlog &= ~vm::fault::EXISTS;
	opts.notify_servers = false;
	log::debug
	{
		log, "Evaluating auth chain for %s in %s events:%zu",
		string_view{room.event_id},
		string_view{room.room_id},
		auth_chain.size(),
	};

	// eval
	m::vm::eval
	{
		auth_chain, opts
	};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Evaluating auth chain for %s in %s :%s",
		string_view{room.event_id},
		string_view{room.room_id},
		e.what(),
	};

	throw;
}

//
// state handler stack
//

void
ircd::m::vm::fetch::state(const event &event,
                          vm::eval &eval,
                          const room &room)
try
{
	const event::prev prev{event};
	const size_t prev_miss
	{
		prev.prev_events_count() - prev.prev_events_exist()
	};

	if(!prev_miss)
		return;

	const auto &[sounding_depth, sounding_idx]
	{
		m::sounding(room.room_id)
	};

	if(at<"depth"_>(event) > sounding_depth)
		return;

	log::dwarning
	{
		log, "%s fetching possible missing state in %s",
		loghead(eval),
		string_view{room.room_id},
	};

	auto futures
	{
		state_fetch(event, eval, room)
	};

	if(!std::distance(begin(futures), end(futures)))
		return;

	auto fetching
	{
		ctx::when_all(begin(futures), end(futures))
	};

	log::warning
	{
		log, "%s fetching %zu missing state events in %s",
		loghead(eval),
		std::distance(begin(futures), end(futures)),
		string_view{room.room_id},
	};

	// yields context
	const bool done
	{
		fetching.wait(seconds(state_timeout), std::nothrow)
	};

	// evaluate results
	size_t good(0), fail(0);
	for(auto &future : futures) try
	{
		m::fetch::result result
		{
			future.get()
		};

		const json::object content
		{
			result
		};

		const json::array &pdus
		{
			content["pdus"]
		};

		auto opts(*eval.opts);
		opts.fetch_prev = false;
		opts.fetch_state = false;
		opts.notify_servers = false;
		vm::eval
		{
			pdus, opts
		};

		++good;
	}
	catch(const ctx::interrupted &)
	{
		throw;
	}
	catch(const std::exception &e)
	{
		++fail;
		log::derror
		{
			log, "%s state eval :%s",
			loghead(eval),
			e.what(),
		};
	}

	log::info
	{
		log, "%s evaluated missing state in %s fetched:%zu good:%zu fail:%zu",
		loghead(eval),
		string_view{room.room_id},
		std::distance(begin(futures), end(futures)),
		good,
		fail,
	};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "%s state fetch in %s :%s",
		loghead(eval),
		string_view{room.room_id},
		e.what(),
	};

	throw;
}

std::forward_list
<
	ircd::ctx::future<ircd::m::fetch::result>
>
ircd::m::vm::fetch::state_fetch(const event &event,
                                vm::eval &eval,
                                const room &room)
{
	feds::opts opts;
	opts.op = feds::op::state;
	opts.event_id = room.event_id;
	opts.room_id = room.room_id;
	opts.arg[0] = "ids";
	opts.exclude_myself = true;
	opts.closure_errors = false;
	opts.nothrow_closure = true;
	log::debug
	{
		log, "%s acquire state event ids in %s...",
		loghead(eval),
		string_view{room.room_id},
	};

	std::set<std::string, std::less<>> req;
	std::forward_list<ctx::future<m::fetch::result>> ret;
	feds::execute(opts, [&eval, &ret, &req]
	(const auto &result)
	{
		const auto each_state_id{[&eval, &ret, &req, &result]
		(const m::event::id &event_id)
		{
			if(m::exists(event_id))
				return;

			auto it(req.lower_bound(event_id));
			if(it != end(req) && *it == event_id)
				return;

			req.emplace_hint(it, event_id);

			m::fetch::opts opts;
			opts.op = m::fetch::op::event;
			opts.room_id = result.request->room_id;
			opts.event_id = event_id;
			ret.emplace_front(m::fetch::start(opts));

			assert(std::distance(begin(ret), end(ret)) <= ssize_t(req.size()));
			log::debug
			{
				log, "%s requesting state event %s off %s in %s reqs:%zu",
				loghead(eval),
				string_view{event_id},
				string_view{result.request->event_id},
				string_view{result.request->room_id},
			};
		}};

		const json::array &auth_chain_ids
		{
			result.object["auth_chain_ids"]
		};

		const json::array &pdu_ids
		{
			result.object["pdu_ids"]
		};

		for(const json::string &auth_chain_id : auth_chain_ids)
			each_state_id(auth_chain_id);

		for(const json::string &pdu_id : pdu_ids)
			each_state_id(pdu_id);

		return true;
	});

	return ret;
}

//
// prev_events handler stack
//

void
ircd::m::vm::fetch::prev(const event &event,
                         vm::eval &eval,
                         const room &room)
{
	const auto &opts{*eval.opts};
	const event::prev prev{event};
	const size_t prev_count
	{
		prev.prev_events_count()
	};

	const size_t prev_exists
	{
		prev.prev_events_exist()
	};

	assert(prev_exists <= prev_count);
	if(prev_count == prev_exists)
		return;

	// Attempt to wait for missing prev_events without issuing fetches here.
	if(prev_wait(event, eval))
		return;

	if(!opts.fetch_prev || !m::vm::fetch::enable)
	{
		prev_check(event, eval);
		return;
	}

	auto futures
	{
		prev_fetch(event, eval, room)
	};

	// At this point one or more prev_events are missing; the fetches were
	// launched asynchronously if the options allowed for it.
	log::dwarning
	{
		log, "%s depth:%ld prev_events:%zu miss:%zu fetching:%zu fetching ...",
		loghead(eval),
		at<"depth"_>(event),
		prev_count,
		prev_count - prev_exists,
		std::distance(begin(futures), end(futures)),
	};

	auto fetching
	{
		ctx::when_all(begin(futures), end(futures))
	};

	const auto timeout
	{
		now<system_point>() + seconds(event_timeout)
	};

	const milliseconds &check_interval
	{
		prev_fetch_check_interval
	};

	// Rather than waiting for all of the events to arrive or for the entire
	// timeout to expire, we check if the sought events made it to the server
	// in the meantime. If so we can drop these requests and bail.
	//TODO: Ideally should be replaced with listener/notification/hook on the
	//TODO: events arriving rather than this coarse sleep cycles.
	while(now<system_point>() < timeout)
	{
		// Wait for an interval to give this loop some iterations.
		if(fetching.wait(check_interval, std::nothrow))
			break;

		// Check for satisfaction.
		if(prev.prev_events_exist() == prev_count)
			return;
	}

	// evaluate results
	for(auto &future : futures) try
	{
		m::fetch::result result
		{
			future.get()
		};

		const json::object content
		{
			result
		};

		const json::array &pdus
		{
			content["pdus"]
		};

		auto opts(*eval.opts);
		opts.fetch_prev = false;
		opts.fetch_state = false;
		opts.notify_servers = false;
		log::debug
		{
			log, "%s fetched %zu pdus; evaluating...",
			loghead(eval),
			pdus.size(),
		};

		vm::eval
		{
			pdus, opts
		};
	}
	catch(const ctx::interrupted &)
	{
		throw;
	}
	catch(const std::exception &e)
	{
		log::derror
		{
			log, "%s prev fetch/eval :%s",
			loghead(eval),
			e.what(),
		};
	}

	// check if result evals have satisfied this eval now; or throw
	prev_check(event, eval);
}

std::forward_list
<
	ircd::ctx::future<ircd::m::fetch::result>
>
ircd::m::vm::fetch::prev_fetch(const event &event,
                               vm::eval &eval,
                               const room &room)
{
	const long room_depth
	{
		m::depth(std::nothrow, room)
	};

	const long viewport_depth
	{
		room_depth - room::events::viewport_size
	};

	std::forward_list
	<
		ctx::future<m::fetch::result>
	>
	ret;
	const event::prev prev{event};
	for(size_t i(0); i < prev.prev_events_count(); ++i) try
	{
		const auto &prev_id
		{
			prev.prev_event(i)
		};

		if(m::exists(prev_id))
			continue;

		const long depth_gap
		{
			std::max(std::abs(at<"depth"_>(event) - room_depth), 1L)
		};

		m::fetch::opts opts;
		opts.op = m::fetch::op::backfill;
		opts.room_id = room.room_id;
		opts.event_id = prev_id;
		opts.backfill_limit = size_t(depth_gap);
		opts.backfill_limit = std::min(opts.backfill_limit, eval.opts->fetch_prev_limit);
		opts.backfill_limit = std::min(opts.backfill_limit, size_t(prev_backfill_limit));
		log::debug
		{
			log, "%s requesting backfill off %s; depth:%ld viewport:%ld room:%ld gap:%ld limit:%zu",
			loghead(eval),
			string_view{prev_id},
			at<"depth"_>(event),
			viewport_depth,
			room_depth,
			depth_gap,
			opts.backfill_limit,
		};

		ret.emplace_front(m::fetch::start(opts));
	}
	catch(const ctx::interrupted &)
	{
		throw;
	}
	catch(const std::exception &e)
	{
		log::derror
		{
			log, "%s requesting backfill off prev %s; depth:%ld :%s",
			loghead(eval),
			string_view{prev.prev_event(i)},
			json::get<"depth"_>(event),
			e.what(),
		};
	}

	return ret;
}

//TODO: Adjust when PDU lookahead/lookaround is fixed in the vm::eval iface.
//TODO: Wait on another eval completion instead of just coarse sleep()'s.
bool
ircd::m::vm::fetch::prev_wait(const event &event,
                              vm::eval &eval)
{
	const auto &opts(*eval.opts);
	const event::prev prev(event);
	const size_t prev_count
	{
		prev.prev_events_count()
	};

	const size_t &wait_count
	{
		ssize_t(opts.fetch_prev_wait_count) >= 0?
			opts.fetch_prev_wait_count:
			size_t(prev_wait_count)
	};

	const milliseconds &wait_time
	{
		opts.fetch_prev_wait_time >= 0ms?
			opts.fetch_prev_wait_time:
			milliseconds(prev_wait_time)
	};

	size_t i(0); while(i < wait_count)
	{
		sleep(milliseconds(++i * wait_time));
		if(prev_count == prev.prev_events_exist())
			return true;
	}

	return false;
}

void
ircd::m::vm::fetch::prev_check(const event &event,
                               vm::eval &eval)
{
	const auto &opts
	{
		*eval.opts
	};

	const event::prev prev
	{
		event
	};

	const size_t prev_exists
	{
		prev.prev_events_exist()
	};

	// Aborts this event if the options want us to guarantee at least one
	// prev_event was fetched and evaluated for this event. This is generally
	// used in conjunction with the fetch_prev_wait option to be effective.
	if(opts.fetch_prev_any && !prev_exists)
		throw vm::error
		{
			vm::fault::EVENT, "Failed to fetch any of the %zu prev_events for %s in %s",
			prev.prev_events_count(),
			string_view{event.event_id},
			json::get<"room_id"_>(event)
		};

	// Aborts this event if the options want us to guarantee ALL of the
	// prev_events were fetched and evaluated for this event.
	if(opts.fetch_prev_all && prev_exists < prev.prev_events_count())
		throw vm::error
		{
			vm::fault::EVENT, "Missing %zu of %zu required prev_events for %s in %s",
			prev_exists,
			prev.prev_events_count(),
			string_view{event.event_id},
			json::get<"room_id"_>(event)
		};
}
