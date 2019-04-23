// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "s_fetch.h"

ircd::mapi::header
IRCD_MODULE
{
    "Event Fetch Unit", ircd::m::fetch::init, ircd::m::fetch::fini
};

decltype(ircd::m::fetch::enable)
ircd::m::fetch::enable
{
	{ "name",     "ircd.m.fetch.enable" },
	{ "default",  false                 },
	{ "persist",  false                 },
};

decltype(ircd::m::fetch::timeout)
ircd::m::fetch::timeout
{
	{ "name",     "ircd.m.fetch.timeout" },
	{ "default",  10L                    },
};

decltype(ircd::m::fetch::hook)
ircd::m::fetch::hook
{
	hook_handler,
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
{
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

	request.wait(seconds(20)); //TODO: conf
	request.get();
	const json::array &array
	{
		request
	};

	std::vector<json::object> events(array.count());
	std::copy(begin(array), end(array), begin(events));
	std::sort(begin(events), end(events), []
	(const json::object &a, const json::object &b)
	{
		return a.at<uint64_t>("depth") < b.at<uint64_t>("depth");
	});

	m::vm::opts vmopts;
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	vmopts.infolog_accept = true;
	vmopts.fetch = false;
	for(const auto &event : events)
		m::vm::eval
		{
			m::event{event}, vmopts
		};
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

	return submit(event_id, room_id);
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

//
// fetch_phase
//

void
ircd::m::fetch::hook_handler(const event &event,
                             vm::eval &eval)
try
{
	assert(eval.opts);
	assert(eval.opts->fetch);
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

	if(eval.copts && my(event))
		return;

	const m::event::id &event_id
	{
		at<"event_id"_>(event)
	};

	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	if(opts.fetch_state_check && !m::exists(room_id))
	{
		// Don't pass event_id in ctor here or m::NOT_FOUND.
		m::room room{room_id};
		room.event_id = event_id;
		if(!opts.fetch_state_wait && !opts.fetch_state)
			throw vm::error
			{
				vm::fault::STATE, "Missing state for room %s",
				string_view{room_id}
			};

		// Auth chain is acquired, validated, and saved by this call or throws.
		if(opts.fetch_auth && bool(m::fetch::enable))
			auth_chain(room, eval.opts->node_id?: event_id.host());
	}

	const event::prev prev
	{
		*eval.event_
	};

	const size_t auth_count
	{
		size(json::get<"auth_events"_>(prev))
	};

	size_t auth_exists(0), auth_fetching(0);
	for(size_t i(0); i < auth_count; ++i)
	{
		const auto &auth_id
		{
			prev.auth_event(i)
		};

		if(!opts.fetch_auth_check)
			continue;

		if(m::exists(auth_id))
		{
			++auth_exists;
			continue;
		}

		const bool can_fetch
		{
			opts.fetch_auth && bool(m::fetch::enable)
		};

		const bool fetching
		{
			can_fetch && start(room_id, auth_id)
		};

		auth_fetching += fetching;
		if(!fetching && !opts.fetch_auth_wait)
			throw vm::error
			{
				vm::fault::EVENT, "Missing auth %s for %s in %s",
				string_view{auth_id},
				json::get<"event_id"_>(*eval.event_),
				json::get<"room_id"_>(*eval.event_)
			};
	}

	const size_t prev_count
	{
		size(json::get<"prev_events"_>(prev))
	};

	size_t prev_exists(0), prev_fetching(0);
	for(size_t i(0); i < prev_count; ++i)
	{
		const auto &prev_id
		{
			prev.prev_event(i)
		};

		if(!opts.fetch_prev_check)
			continue;

		if(m::exists(prev_id))
		{
			++prev_exists;
			continue;
		}

		const bool can_fetch
		{
			opts.fetch_prev && bool(m::fetch::enable)
		};

		const bool fetching
		{
			can_fetch && start(room_id, prev_id)
		};

		prev_fetching += fetching;
		if(!fetching && !opts.fetch_prev_wait)
			throw vm::error
			{
				vm::fault::EVENT, "Missing prev %s for %s in %s",
				string_view{prev_id},
				json::get<"event_id"_>(*eval.event_),
				json::get<"room_id"_>(*eval.event_)
			};
	}

	if(auth_fetching && opts.fetch_auth_wait) for(size_t i(0); i < auth_count; ++i)
	{
		const auto &auth_id
		{
			prev.auth_event(i)
		};

		dock.wait([&auth_id]
		{
			return !requests.count(auth_id);
		});

		if(!m::exists(auth_id))
			throw vm::error
			{
				vm::fault::EVENT, "Failed to fetch auth %s for %s in %s",
				string_view{auth_id},
				json::get<"event_id"_>(*eval.event_),
				json::get<"room_id"_>(*eval.event_)
			};
	}

	size_t prev_fetched(0);
	if(prev_fetching && opts.fetch_prev_wait) for(size_t i(0); i < prev_count; ++i)
	{
		const auto &prev_id
		{
			prev.prev_event(i)
		};

		dock.wait([&prev_id]
		{
			return !requests.count(prev_id);
		});

		prev_fetched += m::exists(prev_id);
	}

	if(opts.fetch_prev && opts.fetch_prev_wait && prev_fetching && !prev_fetched)
		throw vm::error
		{
			vm::fault::EVENT, "Failed to fetch any prev_events for %s in %s",
			json::get<"event_id"_>(*eval.event_),
			json::get<"room_id"_>(*eval.event_)
		};

	if(opts.fetch_prev && opts.fetch_prev_wait && opts.fetch_prev_all && prev_fetched < prev_fetching)
		throw vm::error
		{
			vm::fault::EVENT, "Failed to fetch all required prev_events for %s in %s",
			json::get<"event_id"_>(*eval.event_),
			json::get<"room_id"_>(*eval.event_)
		};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "hook handle %s %s :%s",
		loghead(eval),
		json::get<"event_id"_>(event),
		e.what(),
	};

	throw;
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
				if(r.finished == 0)
					return true;

				if(r.finished && empty(r.buf))
					return true;

				return false;
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
	size_t ret(0);
	auto it(begin(requests));
	while(it != end(requests))
	{
		if(it->finished && empty(it->buf))
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
		for(const auto &request : requests)
			if(timedout(request, now))
				retry(const_cast<fetch::request &>(request));

		return;
	}

	const auto it
	{
		next.get()
	};

	if(it == end(requests))
		return;

	request_handle(it);
}

void
ircd::m::fetch::request_handle(const decltype(requests)::iterator &it)
try
{
	auto &request
	{
		const_cast<fetch::request &>(*it)
	};

	if(!request.started || !request.last || !request.buf)
		return;

	if(request.finished)
		return;

	if(!handle(request))
		return;

	complete.emplace_back(it);
	dock.notify_all();
}
catch(const std::exception &e)
{
	log::error
	{
		log, "fetch eval %s in %s :%s",
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
		dock.notify_all();
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
		request.buf = {};
	}};

	if(request.eptr)
		std::rethrow_exception(request.eptr);

	log::debug
	{
		log, "eval handling %s in %s (r:%zu c:%zu)",
		string_view{request.event_id},
		string_view{request.room_id},
		requests.size(),
		complete.size(),
	};

	const json::object &event
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

	request.last = ircd::time();
	*static_cast<m::v1::event *>(&request) =
	{
		request.event_id, request.buf, std::move(opts)
	};

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
catch(const std::exception &e)
{
	const auto level
	{
		run::level == run::level::QUIT?
			log::DERROR:
			log::ERROR
	};

	log::logf
	{
		log, level, "Failed to start request for %s in %s to '%s' :%s",
		string_view{request.event_id},
		string_view{request.room_id},
		string_view{request.origin},
		e.what()
	};

	server::cancel(request);
	static_cast<m::v1::event *>(&request)->~event();
	request.origin = {};
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
	request.finished = ircd::time();
	dock.notify_all();
}

bool
ircd::m::fetch::timedout(const request &request,
                         const time_t &now)
{
	if(!request.started || request.finished)
		return false;

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
