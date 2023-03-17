// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
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
	static void prev_eval(const event &, vm::eval &, ctx::future<m::fetch::result> &, const system_point &);
	static void prev(const event &, vm::eval &, const room &);
	static std::forward_list<ctx::future<m::fetch::result>> state_fetch(const event &, vm::eval &, const room &);
	static void state(const event &, vm::eval &, const room &);
	static void auth_chain_eval(const event &, vm::eval &, const room &, const json::array &, const string_view &);
	static void auth_chain(const event &, vm::eval &, const room &);
	static void auth(const event &, vm::eval &, const room &);
	static void handle(const event &, vm::eval &);

	extern conf::item<milliseconds> prev_preempt_time;
	extern conf::item<milliseconds> prev_wait_time;
	extern conf::item<size_t> prev_backfill_limit;
	extern conf::item<seconds> event_timeout;
	extern conf::item<seconds> state_timeout;
	extern conf::item<seconds> auth_timeout;
	extern conf::item<bool> enable;
	extern hookfn<vm::eval &> auth_hook;
	extern hookfn<vm::eval &> prev_hook;
	extern hookfn<vm::eval &> state_hook;
	extern log::log log;
}

namespace ircd::m::vm::fetch::stats
{
	using ircd::stats::item;

	extern item<uint64_t> prev_noempts;
	extern item<uint64_t> prev_preempts;
	extern item<uint64_t> prev_evals;
	extern item<uint64_t> prev_fetched;
	extern item<uint64_t> prev_fetches;
	extern item<uint64_t> auth_evals;
	extern item<uint64_t> auth_fetched;
	extern item<uint64_t> auth_fetches;
	extern item<uint64_t> state_evals;
	extern item<uint64_t> state_fetched;
	extern item<uint64_t> state_fetches;
}

decltype(ircd::m::vm::fetch::log)
ircd::m::vm::fetch::log
{
	"m.vm.fetch"
};

decltype(ircd::m::vm::fetch::auth_hook)
ircd::m::vm::fetch::auth_hook
{
	handle,
	{
		{ "_site",  "vm.fetch.auth" }
	}
};

decltype(ircd::m::vm::fetch::prev_hook)
ircd::m::vm::fetch::prev_hook
{
	handle,
	{
		{ "_site",  "vm.fetch.prev" }
	}
};

decltype(ircd::m::vm::fetch::state_hook)
ircd::m::vm::fetch::state_hook
{
	handle,
	{
		{ "_site",  "vm.fetch.state" }
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

decltype(ircd::m::vm::fetch::prev_wait_time)
ircd::m::vm::fetch::prev_wait_time
{
	{ "name",     "ircd.m.vm.fetch.prev.wait.time" },
	{ "default",  750L                             },
};

decltype(ircd::m::vm::fetch::prev_preempt_time)
ircd::m::vm::fetch::prev_preempt_time
{
	{ "name",     "ircd.m.vm.fetch.prev.preempt.time" },
	{ "default",  5000L                               },
};

decltype(ircd::m::vm::fetch::stats::state_fetches)
ircd::m::vm::fetch::stats::state_fetches
{
	{ "name", "ircd.m.vm.fetch.state.fetches" },
};

decltype(ircd::m::vm::fetch::stats::state_fetched)
ircd::m::vm::fetch::stats::state_fetched
{
	{ "name", "ircd.m.vm.fetch.state.fetched" },
};

decltype(ircd::m::vm::fetch::stats::state_evals)
ircd::m::vm::fetch::stats::state_evals
{
	{ "name", "ircd.m.vm.fetch.state.evals" },
};

decltype(ircd::m::vm::fetch::stats::auth_fetches)
ircd::m::vm::fetch::stats::auth_fetches
{
	{ "name", "ircd.m.vm.fetch.auth.fetches" },
};

decltype(ircd::m::vm::fetch::stats::auth_fetched)
ircd::m::vm::fetch::stats::auth_fetched
{
	{ "name", "ircd.m.vm.fetch.auth.fetched" },
};

decltype(ircd::m::vm::fetch::stats::auth_evals)
ircd::m::vm::fetch::stats::auth_evals
{
	{ "name", "ircd.m.vm.fetch.auth.evals" },
};

decltype(ircd::m::vm::fetch::stats::prev_fetches)
ircd::m::vm::fetch::stats::prev_fetches
{
	{ "name", "ircd.m.vm.fetch.prev.fetches" },
};

decltype(ircd::m::vm::fetch::stats::prev_fetched)
ircd::m::vm::fetch::stats::prev_fetched
{
	{ "name", "ircd.m.vm.fetch.prev.fetched" },
};

decltype(ircd::m::vm::fetch::stats::prev_evals)
ircd::m::vm::fetch::stats::prev_evals
{
	{ "name", "ircd.m.vm.fetch.prev.evals" },
};

decltype(ircd::m::vm::fetch::stats::prev_preempts)
ircd::m::vm::fetch::stats::prev_preempts
{
	{ "name", "ircd.m.vm.fetch.prev.preempts" },
};

decltype(ircd::m::vm::fetch::stats::prev_noempts)
ircd::m::vm::fetch::stats::prev_noempts
{
	{ "name", "ircd.m.vm.fetch.prev.noempts" },
};

//
// fetch_phase
//

void
ircd::m::vm::fetch::handle(const event &event,
                           vm::eval &eval)
try
{
	if(eval.room_internal)
		return;

	assert(eval.opts);
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

	if(eval.phase == phase::FETCH_AUTH)
		return auth(event, eval, room);

	if(eval.phase == phase::FETCH_PREV)
		return prev(event, eval, room);

	if(eval.phase == phase::FETCH_STATE)
		return state(event, eval, room);
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

{
	// Count how many of the auth_events provided exist locally.
	const auto &opts{*eval.opts};
	const event::auth prev
	{
		event
	};

	const size_t auth_count
	{
		prev.auth_events_count()
	};

	const size_t auth_exists
	{
		prev.auth_events_exist()
	};

	assert(auth_exists <= auth_count);
	if(auth_exists != auth_count) try
	{
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

		// This is a blocking call to recursively fetch and evaluate the auth_chain
		// for this event. Upon return all of the auth_events for this event will
		// have themselves been fetched and auth'ed recursively.
		auth_chain(event, eval, room);
	}
	catch(const std::exception &e)
	{
		const ctx::exception_handler eh;
		throw vm::error
		{
			vm::fault::AUTH, "Failed to fetch %zu of %zu auth_events :%s",
			auth_count - prev.auth_events_exist(),
			auth_count,
			e.what()
		};
	}
}

void
ircd::m::vm::fetch::auth_chain(const event &event,
                               vm::eval &eval,
                               const room &room)
try
{
	assert(eval.opts);

	// Figure out a remote hint as the primary target to request the missing
	// auth events from; if provided, m::fetch will ask this remote first. We
	// try to use the eval.node_id, which is set to a server that is conducting
	// the eval (i.e in a /send/ or when processing some response data from
	// them); next we try the origin of the event itself. These remotes are
	// most likely to provide a satisfying response.
	const string_view hint
	{
		eval.opts->node_id && !my_host(eval.opts->node_id)?
			eval.opts->node_id:

		event.event_id.host() && !my_host(event.event_id.host())?
			event.event_id.host():

		!my_host(json::get<"origin"_>(event))?
			string_view(json::get<"origin"_>(event)):

		room.room_id.host() && !my_host(room.room_id.host())?
			room.room_id.host():

		string_view{}
	};

	log::debug
	{
		log, "Fetching auth chain for %s in %s hint:%s",
		string_view{room.event_id},
		string_view{room.room_id},
		hint,
	};

	// send
	auto future
	{
		m::fetch::start(
		{
			.op = m::fetch::op::auth,
			.room_id = room.room_id,
			.event_id = room.event_id,
			.hint = hint,
			.check_hashes = false,
		})
	};

	// recv
	stats::auth_fetches++;
	const auto result
	{
		future.get(seconds(auth_timeout))
	};

	stats::auth_fetched++;
	const json::object response
	{
		result
	};

	// parse
	const json::array &auth_chain
	{
		response["auth_chain"]
	};

	auth_chain_eval(event, eval, room, auth_chain, result.origin);
	stats::auth_evals++;
}
catch(const vm::error &e)
{
	throw;
}
catch(const std::exception &e)
{
	log::logf
	{
		log, eval.opts->auth? log::ERROR: log::DERROR,
		"Fetching auth chain for %s in %s :%s",
		string_view{room.event_id},
		string_view{room.room_id},
		e.what(),
	};

	// Stop propagation if auth not required but fetch attempted anyway
	if(!eval.opts->auth)
		return;

	throw;
}

void
ircd::m::vm::fetch::auth_chain_eval(const event &event,
                                    vm::eval &eval,
                                    const room &room,
                                    const json::array &auth_chain_,
                                    const string_view &origin)
try
{
	assert(eval.opts);
	auto opts(*eval.opts);
	opts.fetch = false;
	opts.infolog_accept = true;
	opts.warnlog &= ~vm::fault::EXISTS;
	opts.notify_servers = false;
	opts.node_id = origin;

	std::vector<m::event> auth_chain
	(
		std::begin(auth_chain_), std::end(auth_chain_)
	);

	// pre-sort here and indicate that to eval.
	std::sort(begin(auth_chain), end(auth_chain));
	opts.ordered = true;

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
	const auto &opts{*eval.opts};
	const event::prev prev
	{
		event
	};

	const bool prev_exist
	{
		prev.prev_exist()
	};

	if(opts.fetch_state_any)
		if(prev_exist && prev.prev_events_count() == prev.prev_events_exist())
			return;

	if(likely(!opts.fetch_state_any))
		if(prev_exist)
			return;

	if(likely(!opts.fetch_state_shallow))
	{
		const auto &[sounding_depth, sounding_idx]
		{
			m::sounding(room.room_id)
		};

		if(at<"depth"_>(event) > sounding_depth)
			return;
	}

	stats::state_fetches++;
	log::dwarning
	{
		log, "%s fetching possible missing state in %s",
		loghead(eval),
		string_view{room.room_id},
	};

	struct m::acquire::opts acq_opts;
	acq_opts.room = room;
	acq_opts.head = false;
	acq_opts.history = false;
	acq_opts.state = true;
	acq_opts.hint = opts.node_id;
	m::acquire acq
	{
		acq_opts
	};

	//TODO: XXX
	stats::state_fetched++;
	log::info
	{
		log, "%s evaluated missing state in %s fetched:-- good:-- fail:--",
		loghead(eval),
		string_view{room.room_id},
		0,
		0,
		0,
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
	{
		stats::prev_noempts += prev_count;
		return;
	}

	if(!m::vm::fetch::enable)
	{
		// No fetches will take place; only check if satisfied.
		prev_check(event, eval);
		return;
	}

	// Launch fetches for missing prev events
	auto fetching
	{
		prev_fetch(event, eval, room)
	};

	const auto fetching_count
	{
		std::distance(begin(fetching), end(fetching))
	};

	// At this point one or more prev_events are missing; the fetches were
	// launched asynchronously if the options allowed for it.
	stats::prev_fetches += fetching_count;
	log::dwarning
	{
		log, "%s depth:%ld prev_events:%zu miss:%zu fetching:%zu fetching ...",
		loghead(eval),
		at<"depth"_>(event),
		prev_count,
		prev_count - prev_exists,
		fetching_count,
	};

	// Rather than waiting for all of the events to arrive or for the entire
	// timeout to expire, we check if the sought events made it to the server
	// in the meantime. If so we can drop these fetches and bail.
	std::optional<vm::notify::future> evaling[prev_count];
	for(size_t i(0); i < prev_count; ++i)
		evaling[i].emplace(prev.prev_event(i));

	// Either all of the fetches are done and we can start evaluating or all
	// of the events arrived elsehow and we don't need any of the fetches.
	// XXX: Ideally this could be refactored with mix-and-match granularity
	// but at this time it's unknown if there's practical benefit.
	ctx::future<> when[]
	{
		ctx::when_all(begin(fetching), end(fetching)),
		ctx::when_all(evaling, evaling + prev_count, []
		(auto &optional) -> ctx::future<> &
		{
			return optional->value();
		}),
	};

	// Represents one of the two outcomes.
	auto future
	{
		ctx::when_any(begin(when), end(when))
	};

	const auto prev_wait_until
	{
		now<system_point>() + milliseconds(prev_preempt_time)
	};

	// Wait for one of the two outcomes.
	const bool finished
	{
		future.wait_until(prev_wait_until, std::nothrow)
	};

	// Check for satisfaction.
	if(prev.prev_events_exist() == prev_count)
	{
		stats::prev_preempts += prev_count;
		assert(finished);
		return;
	}

	const auto event_wait_until
	{
		now<system_point>() + seconds(event_timeout)
	};

	// If we're not satisfied we commit to evaluating the fetches.
	for(auto &fetch : fetching)
		prev_eval(event, eval, fetch, event_wait_until);

	// check if result evals have satisfied this eval now; or throw
	prev_check(event, eval);
}

void
ircd::m::vm::fetch::prev_eval(const event &event,
                              vm::eval &eval,
                              ctx::future<m::fetch::result> &future,
                              const system_point &until)
try
{
	m::fetch::result result
	{
		future.get_until(until)
	};

	stats::prev_fetched++;
	const json::object content
	{
		result
	};

	const json::array &pdus
	{
		content["pdus"]
	};

	auto opts(*eval.opts);
	opts.phase.set(m::vm::phase::FETCH_PREV, false);
	opts.phase.set(m::vm::phase::FETCH_STATE, false);
	opts.notify_servers = false;
	opts.node_id = result.origin;
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

	stats::prev_evals++;
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

		const auto backfill_max
		{
			std::min(size_t(prev_backfill_limit), eval.opts->fetch_prev_limit)
		};

		const auto backfill_limit
		{
			std::min(size_t(depth_gap), backfill_max)
		};

		const string_view hint
		{
			eval.opts->node_id && !my_host(eval.opts->node_id)?
				eval.opts->node_id:

			prev_id.host() && !my_host(prev_id.host())?
				prev_id.host():

			!my_host(json::get<"origin"_>(event))?
				string_view(json::get<"origin"_>(event)):

			room.room_id.host() && !my_host(room.room_id.host())?
				room.room_id.host():

			string_view{}
		};

		log::debug
		{
			log, "%s requesting backfill off %s; "
			"depth:%ld viewport:%ld room:%ld gap:%ld limit:%zu hint:%s",
			loghead(eval),
			string_view{prev_id},
			at<"depth"_>(event),
			viewport_depth,
			room_depth,
			depth_gap,
			backfill_limit,
			hint,
		};

		ret.emplace_front(m::fetch::start(
		{
			.op = m::fetch::op::backfill,
			.room_id = room.room_id,
			.event_id = prev_id,
			.hint = hint,
			.backfill_limit = backfill_limit,
		}));
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

bool
ircd::m::vm::fetch::prev_wait(const event &event,
                              vm::eval &eval)
{
	const auto &opts(*eval.opts);
	const event::prev prev(event);
	const milliseconds &wait_time
	{
		opts.fetch_prev_wait_time >= 0ms?
			opts.fetch_prev_wait_time:
			milliseconds(prev_wait_time)
	};

	const auto ids
	{
		prev.prev_events_count()
	};

	assume(ids <= event::prev::MAX);
	event::id buf[event::prev::MAX];
	const vector_view<const event::id> prev_id
	(
		prev.ids(buf)
	);

	const auto exists
	{
		notify::wait(prev_id, wait_time)
	};

	const bool obtained
	{
		exists == ids
	};

	return obtained;
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
