// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "m_fetch.h"

ircd::mapi::header
IRCD_MODULE
{
    "Event Fetch Unit", ircd::m::fetch::init, ircd::m::fetch::fini
};

decltype(ircd::m::fetch::enable)
ircd::m::fetch::enable
{
	{ "name",     "ircd.m.fetch.enable" },
	{ "default",  true                  },
};

decltype(ircd::m::fetch::timeout)
ircd::m::fetch::timeout
{
	{ "name",     "ircd.m.fetch.timeout" },
	{ "default",  5L                     },
};

decltype(ircd::m::fetch::auth_timeout)
ircd::m::fetch::auth_timeout
{
	{ "name",     "ircd.m.fetch.auth.timeout" },
	{ "default",  15L                         },
};

decltype(ircd::m::fetch::requests_max)
ircd::m::fetch::requests_max
{
	{ "name",     "ircd.m.fetch.requests.max" },
	{ "default",  256L                        },
};

decltype(ircd::m::fetch::hook)
ircd::m::fetch::hook
{
	hook_handle,
	{
		{ "_site",  "vm.fetch" }
	}
};

decltype(ircd::m::fetch::request_context)
ircd::m::fetch::request_context
{
	"m::fetch req", 512_KiB, &request_worker, context::POST
};

decltype(ircd::m::fetch::eval_context)
ircd::m::fetch::eval_context
{
	"m::fetch eval", 512_KiB, &eval_worker, context::POST
};

decltype(ircd::m::fetch::complete)
ircd::m::fetch::complete;

decltype(ircd::m::fetch::rooms)
ircd::m::fetch::rooms;

decltype(ircd::m::fetch::requests)
ircd::m::fetch::requests;

decltype(ircd::m::fetch::dock)
ircd::m::fetch::dock;

//
// init
//

void
ircd::m::fetch::init()
{
}

void
ircd::m::fetch::fini()
{
	request_context.terminate();
	eval_context.terminate();
	request_context.join();
	eval_context.join();
	requests.clear();
	complete.clear();

	assert(requests.empty());
	assert(complete.empty());
}

//
// fetch_phase
//

void
ircd::m::fetch::hook_handle(const event &event,
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
		log, "%s %s %s ac:%zu ae:%zu pc:%zu pe:%zu pf:%zu",
		loghead(eval),
		string_view{event.event_id},
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
		log, "%s %s :%s",
		loghead(eval),
		string_view{event.event_id},
		e.what(),
	};

	throw;
}

void
ircd::m::fetch::hook_handle_auth(const event &event,
                                 vm::eval &eval,
                                 evaltab &tab,
                                 const room &room)

{
	// Count how many of the auth_events provided exist locally.
	const auto &opts{*eval.opts};
	const event::prev prev{event};
	tab.auth_count = size(json::get<"auth_events"_>(prev));
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
		log, "%s %s auth_events:%zu hit:%zu miss:%zu",
		loghead(eval),
		string_view{event.event_id},
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
	if(!opts.fetch_auth || !bool(m::fetch::enable) || !remote)
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
ircd::m::fetch::hook_handle_prev(const event &event,
                                 vm::eval &eval,
                                 evaltab &tab,
                                 const room &room)
{
	const auto &opts{*eval.opts};
	const event::prev prev{event};
	tab.prev_count = size(json::get<"prev_events"_>(prev));
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
			opts.fetch_prev && bool(m::fetch::enable)
		};

		const bool fetching
		{
			can_fetch && start(room.room_id, prev_id)
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
		log, "%s %s prev_events:%zu hit:%zu miss:%zu fetching:%zu",
		loghead(eval),
		string_view{event.event_id},
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

		dock.wait([&prev_id]
		{
			return !requests.count(prev_id);
		});

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

///////////////////////////////////////////////////////////////////////////////
//
// m/fetch.h
//

namespace ircd::m::fetch
{
	static m::event::id::buf _head(const m::feds::opts &);
	static std::map<std::string, size_t> _heads(const m::feds::opts &);
	static void handle_state_ids(const m::room &, const m::feds::result &);
}

bool
IRCD_MODULE_EXPORT
ircd::m::fetch::synchronize(const m::room &room)
{
	m::feds::opts opts;
	opts.op = m::feds::op::head;
	opts.room_id = room.room_id;
	opts.event_id = room.event_id;
	opts.nothrow_closure = true;
	opts.closure_errors = false;

	return true;
}

void
IRCD_MODULE_EXPORT
ircd::m::fetch::state_ids(const room &room)
{
	m::feds::opts opts;
	opts.room_id = room.room_id;
	opts.event_id = room.event_id;
	opts.timeout = seconds(10); //TODO: conf

	m::event::id::buf event_id_buf;
	if(!opts.event_id)
	{
		log::debug
		{
			log, "No event_id supplied; fetching heads for %s...",
			string_view{room.room_id},
		};

		event_id_buf = _head(opts);
		opts.event_id = event_id_buf;
	}

	opts.arg[0] = "ids";
	opts.op = m::feds::op::state;
	opts.timeout = seconds(20); //TODO: conf
	m::feds::acquire(opts, [&room]
	(const auto &result)
	{
		handle_state_ids(room, result);
		return true;
	});
}

ircd::m::event::id::buf
ircd::m::fetch::_head(const m::feds::opts &opts)
{
	const auto heads
	{
		_heads(opts)
	};

	const auto it
	{
		std::max_element(begin(heads), end(heads), []
		(const auto &a, const auto &b)
		{
			return a.second < b.second;
		})
	};

	return it != end(heads)?
		event::id::buf{it->first}:
		event::id::buf{};

}

std::map<std::string, size_t>
ircd::m::fetch::_heads(const m::feds::opts &opts_)
{
	auto opts(opts_);
	opts.op = m::feds::op::head;

	std::map<std::string, size_t> heads;
	m::feds::acquire(opts, [&heads]
	(const auto &result)
	{
		if(result.eptr)
			return true;

		const json::object &event{result.object["event"]};
		const m::event::prev prev{event};
		for(size_t i(0); i < prev.prev_events_count(); ++i)
		{
			// check for dups to prevent result bias.
			const auto &prev_event_id(prev.prev_event(i));
			for(size_t j(0); j < prev.prev_events_count(); ++j)
				if(i != j && prev.prev_event(j) == prev_event_id)
					return true;

			++heads[prev_event_id];
		}

		return true;
	});

	return heads;
}

void
ircd::m::fetch::handle_state_ids(const m::room &room,
                                 const m::feds::result &result)
try
{
	if(result.eptr)
		std::rethrow_exception(result.eptr);

	const json::array &ids
	{
		result.object["pdu_ids"]
	};

	log::debug
	{
		log, "Got %zu state_ids for %s from '%s'",
		ids.size(),
		string_view{room.room_id},
		string_view{result.origin},
	};

	size_t count(0);
	for(const json::string &event_id : ids)
		count += fetch::prefetch(room.room_id, event_id);

	if(count)
		log::debug
		{
			log, "Prefetched %zu of %zu state_ids for %s from '%s'",
			count,
			ids.size(),
			string_view{room.room_id},
			string_view{result.origin},
		};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Requesting state_ids for %s from '%s' :%s",
		string_view{room.room_id},
		result.origin,
		e.what(),
	};
}

void
IRCD_MODULE_EXPORT
ircd::m::fetch::auth_chain(const room &room,
                           const net::hostport &remote)
try
{
	thread_local char rembuf[64];
	log::debug
	{
		log, "Fetching auth chain for %s in %s from %s",
		string_view{room.event_id},
		string_view{room.room_id},
		string(rembuf, remote),
	};

	m::v1::event_auth::opts opts;
	opts.remote = remote;
	opts.dynamic = true;
	const unique_buffer<mutable_buffer> buf
	{
		8_KiB
	};

	m::v1::event_auth request
	{
		room.room_id, room.event_id, buf, std::move(opts)
	};

	request.wait(seconds(auth_timeout));
	request.get();
	const json::array events
	{
		request
	};

	log::debug
	{
		log, "Evaluating %zu auth events in chain for %s in %s from %s",
		events.size(),
		string_view{room.event_id},
		string_view{room.room_id},
		string(rembuf, remote),
	};

	m::vm::opts vmopts;
	vmopts.infolog_accept = true;
	vmopts.fetch_prev_check = false;
	vmopts.fetch_state_check = false;
	vmopts.warnlog &= ~vm::fault::EXISTS;
	m::vm::eval
	{
		events, vmopts
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

bool
IRCD_MODULE_EXPORT
ircd::m::fetch::prefetch(const m::room::id &room_id,
                         const m::event::id &event_id)
{
	if(m::exists(event_id))
		return false;

	return start(room_id, event_id);
}

bool
IRCD_MODULE_EXPORT
ircd::m::fetch::start(const m::room::id &room_id,
                      const m::event::id &event_id)
{
	ircd::run::changed::dock.wait([]
	{
		return run::level == run::level::RUN ||
		       run::level == run::level::QUIT;
	});

	if(unlikely(run::level != run::level::RUN))
		throw m::UNAVAILABLE
		{
			"Cannot fetch %s in %s in runlevel '%s'",
			string_view{event_id},
			string_view{room_id},
			reflect(run::level)
		};

	if(count() > size_t(requests_max))
		return false;

	return submit(event_id, room_id);
}

size_t
IRCD_MODULE_EXPORT
ircd::m::fetch::count()
{
	size_t ret(0);
	for_each([&ret](const auto &request)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
IRCD_MODULE_EXPORT
ircd::m::fetch::for_each(const std::function<bool (request &)> &closure)
{
	for(auto &request : requests)
		if(!closure(const_cast<fetch::request &>(request)))
			return false;

	return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// s_fetch.h
//

template<class... args>
bool
ircd::m::fetch::submit(const m::event::id &event_id,
                       const m::room::id &room_id,
                       const size_t &bufsz,
                       args&&... a)
try
{
	assert(room_id && event_id);
	auto it(requests.lower_bound(string_view(event_id)));
	if(it != end(requests) && it->event_id == event_id)
	{
		assert(it->room_id == room_id);
		return false;
	}

	it = requests.emplace_hint(it, room_id, event_id, bufsz, std::forward<args>(a)...);
	auto &request(const_cast<fetch::request &>(*it)); try
	{
		while(!start(request))
			request.origin = {};
	}
	catch(const std::exception &e)
	{
		throw;
	}

	return true;
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "Failed to start any fetch for %s in %s :%s",
		string_view{event_id},
		string_view{room_id},
		e.what(),
	};

	return false;
}

//
// request worker
//

void
ircd::m::fetch::request_worker()
try
{
	while(1)
	{
		dock.wait([]
		{
			return std::any_of(begin(requests), end(requests), []
			(const request &r)
			{
				return r.finished <= 0;
			});
		});

		if(request_cleanup())
			continue;

		if(requests.empty())
			continue;

		request_handle();
	}
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "fetch request worker :%s",
		e.what()
	};

	throw;
}

size_t
ircd::m::fetch::request_cleanup()
{
	// assert that there is no race starting from here.
	const ctx::critical_assertion ca;

	size_t ret(0);
	auto it(begin(requests));
	while(it != end(requests))
	{
		if(it->finished == -1)
		{
			it = requests.erase(it);
			++ret;
		}
		else ++it;
	}

	return ret;
}

void
ircd::m::fetch::request_handle()
{
	auto next
	{
		ctx::when_any(requests.begin(), requests.end())
	};

	if(!next.wait(seconds(timeout), std::nothrow))
	{
		const auto now(ircd::time());
		for(auto it(begin(requests)); it != end(requests); ++it)
		{
			auto &request(const_cast<fetch::request &>(*it));
			if(request.finished < 0 || request.last == std::numeric_limits<time_t>::max())
				continue;

			if(request.finished == 0 && timedout(request, now))
			{
				retry(request);
				continue;
			}

			if(request.finished > 0 && timedout(request, now))
			{
				request.finished = -1;
				continue;
			}
		}

		request_cleanup();
		return;
	}

	const auto it
	{
		next.get()
	};

	if(it == end(requests))
		return;

	request_handle(it);
	dock.notify_all();
}

void
ircd::m::fetch::request_handle(const decltype(requests)::iterator &it)
try
{
	auto &request
	{
		const_cast<fetch::request &>(*it)
	};

	if(!request.started || !request.last || request.finished < 0)
		return;

	if(!request.finished && !handle(request))
		return;

	assert(request.finished);
	if(request.eptr)
	{
		request.finished = -1;
		std::rethrow_exception(request.eptr);
		__builtin_unreachable();
	}

	assert(!request.eptr);
	request.last = std::numeric_limits<time_t>::max();
	complete.emplace_back(it);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "request %s in %s :%s",
		string_view{it->event_id},
		string_view{it->room_id},
		e.what()
	};
}

//
// eval worker
//

void
ircd::m::fetch::eval_worker()
try
{
	while(1)
	{
		dock.wait([]
		{
			return !complete.empty();
		});

		eval_handle();
	}
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "fetch eval worker :%s",
		e.what()
	};

	throw;
}

void
ircd::m::fetch::eval_handle()
{
	assert(!complete.empty());
	const unwind pop{[]
	{
		assert(!complete.empty());
		complete.pop_front();
	}};

	const auto it
	{
		complete.front()
	};

	eval_handle(it);
}

void
ircd::m::fetch::eval_handle(const decltype(requests)::iterator &it)
try
{
	auto &request
	{
		const_cast<fetch::request &>(*it)
	};

	const unwind free{[&request]
	{
		request.finished = -1;
		dock.notify_all();
	}};

	assert(!request.eptr);
	log::debug
	{
		log, "eval handling %s in %s (r:%zu c:%zu)",
		string_view{request.event_id},
		string_view{request.room_id},
		requests.size(),
		complete.size(),
	};

	const json::object event
	{
		request
	};

	m::vm::opts opts;
	opts.infolog_accept = true;
	opts.fetch_prev = false;
	opts.fetch_state_wait = false;
	opts.fetch_auth_wait = false;
	opts.fetch_prev_wait = false;
	m::vm::eval
	{
		m::event{event}, opts
	};
}
catch(const std::exception &e)
{
	auto &request
	{
		const_cast<fetch::request &>(*it)
	};

	if(!request.eptr)
		request.eptr = std::current_exception();

	log::error
	{
		log, "fetch eval %s in %s :%s",
		string_view{request.event_id},
		string_view{request.room_id},
		e.what()
	};
}

//
// fetch::request
//

bool
ircd::m::fetch::start(request &request)
{
	m::v1::event::opts opts;
	opts.dynamic = true;
	if(!request.origin)
		select_random_origin(request);

	opts.remote = request.origin;
	return start(request, opts);
}

bool
ircd::m::fetch::start(request &request,
                      m::v1::event::opts &opts)
try
{
	assert(request.finished == 0);
	if(!request.started)
		request.started = ircd::time();

	request.last = ircd::time(); try
	{
		*static_cast<m::v1::event *>(&request) =
		{
			request.event_id, request.buf, std::move(opts)
		};
	}
	catch(...)
	{
		cancel(request);
		throw;
	}

	log::debug
	{
		log, "Started request for %s in %s from '%s'",
		string_view{request.event_id},
		string_view{request.room_id},
		string_view{request.origin},
	};

	dock.notify_all();
	return true;
}
catch(const http::error &e)
{
	log::logf
	{
		log, run::level == run::level::QUIT? log::DERROR: log::ERROR,
		"Failed to start request for %s in %s to '%s' :%s %s",
		string_view{request.event_id},
		string_view{request.room_id},
		string_view{request.origin},
		e.what(),
		e.content,
	};

	return false;
}
catch(const std::exception &e)
{
	log::logf
	{
		log, run::level == run::level::QUIT? log::DERROR: log::ERROR,
		"Failed to start request for %s in %s to '%s' :%s",
		string_view{request.event_id},
		string_view{request.room_id},
		string_view{request.origin},
		e.what()
	};

	return false;
}

ircd::string_view
ircd::m::fetch::select_random_origin(request &request)
{
	const m::room::origins origins
	{
		request.room_id
	};

	// copies randomly selected origin into the attempted set.
	const auto closure{[&request]
	(const string_view &origin)
	{
		select_origin(request, origin);
	}};

	// Tests if origin is potentially viable
	const auto proffer{[&request]
	(const string_view &origin)
	{
		// Don't want to request from myself.
		if(my_host(origin))
			return false;

		// Don't want to use a peer we already tried and failed with.
		if(request.attempted.count(origin))
			return false;

		// Don't want to use a peer marked with an error by ircd::server
		if(ircd::server::errmsg(origin))
			return false;

		return true;
	}};

	request.origin = {};
	if(!origins.random(closure, proffer) || !request.origin)
		throw m::NOT_FOUND
		{
			"Cannot find any server to fetch %s in %s",
			string_view{request.event_id},
			string_view{request.room_id},
		};

	return request.origin;
}

ircd::string_view
ircd::m::fetch::select_origin(request &request,
                              const string_view &origin)
{
	const auto iit
	{
		request.attempted.emplace(std::string{origin})
	};

	request.origin = *iit.first;
	return request.origin;
}

bool
ircd::m::fetch::handle(request &request)
{
	request.wait(); try
	{
		const auto code
		{
			request.get()
		};

		log::debug
		{
			log, "%u %s for %s in %s from '%s'",
			uint(code),
			status(code),
			string_view{request.event_id},
			string_view{request.room_id},
			string_view{request.origin}
		};
	}
	catch(...)
	{
		request.eptr = std::current_exception();

		log::derror
		{
			log, "Failure for %s in %s from '%s' :%s",
			string_view{request.event_id},
			string_view{request.room_id},
			string_view{request.origin},
			what(request.eptr),
		};
	}

	if(!request.eptr)
		finish(request);
	else
		retry(request);

	return request.finished;
}

void
ircd::m::fetch::retry(request &request)
try
{
	assert(!request.finished);
	assert(request.started && request.last);
	server::cancel(request);
	request.eptr = std::exception_ptr{};
	request.origin = {};
	start(request);
}
catch(...)
{
	request.eptr = std::current_exception();
	finish(request);
}

void
ircd::m::fetch::finish(request &request)
{
	assert(request.started);
	request.finished = ircd::time();
}

bool
ircd::m::fetch::timedout(const request &request,
                         const time_t &now)
{
	assert(request.started && request.finished == 0 && request.last != 0);
	return request.last + seconds(timeout).count() < now;
}

bool
ircd::m::fetch::operator<(const request &a,
                          const request &b)
noexcept
{
	return a.event_id < b.event_id;
}

bool
ircd::m::fetch::operator<(const request &a,
                          const string_view &b)
noexcept
{
	return a.event_id < b;
}

bool
ircd::m::fetch::operator<(const string_view &a,
                          const request &b)
noexcept
{
	return a < b.event_id;
}

//
// request::request
//

ircd::m::fetch::request::request(const m::room::id &room_id,
                                 const m::event::id &event_id,
                                 const size_t &bufsz)
:room_id{room_id}
,event_id{event_id}
,buf{bufsz}
{
}
