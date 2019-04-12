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

//
// fetch_phase
//

void
ircd::m::fetch::hook_handler(const event &event,
                             vm::eval &eval)
{
	assert(eval.opts);
	const auto &opts{*eval.opts};
	const auto &type
	{
		at<"type"_>(event)
	};

	if(type == "m.room.create")
		return;

	const m::event::id &event_id
	{
		at<"event_id"_>(event)
	};

	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	if(opts.state_check_exists && !m::exists(room_id))
	{
		// Don't pass event_id in ctor here or m::NOT_FOUND.
		m::room room{room_id};
		room.event_id = event_id;

		if(opts.state_must_exist && !opts.state_wait && !opts.state_fetch)
			throw vm::error
			{
				vm::fault::STATE, "Missing state for room %s",
				string_view{room_id}
			};

		if(opts.auth_chain_fetch)
			fetch::auth_chain(room, room_id.host()); //TODO: XXX
	}

	const event::prev prev
	{
		*eval.event_
	};

	const size_t prev_count
	{
		size(json::get<"prev_events"_>(prev))
	};

	size_t prev_exists(0);
	for(size_t i(0); i < prev_count; ++i)
	{
		const auto &prev_id
		{
			prev.prev_event(i)
		};

		if(!opts.prev_check_exists)
			continue;

		if(m::exists(prev_id))
		{
			++prev_exists;
			continue;
		}

		const bool can_fetch
		{
			opts.prev_fetch && bool(m::fetch::enable)
		};

		if((!can_fetch || !opts.prev_wait) && opts.prev_must_all_exist)
			throw vm::error
			{
				vm::fault::EVENT, "Missing prev %s in %s in %s",
				string_view{prev_id},
				json::get<"event_id"_>(*eval.event_),
				json::get<"room_id"_>(*eval.event_)
			};

		if(can_fetch)
			start(room_id, prev_id);

		if(can_fetch && opts.prev_wait)
			dock.wait([&prev_id]
			{
				return !requests.count(prev_id);
			});
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// m/fetch.h
//

namespace ircd::m::fetch
{
	static void handle_headfill(const m::room &, const m::feds::result &);
}

void
IRCD_MODULE_EXPORT
ircd::m::fetch::headfill(const room &room)
{
	m::feds::opts opts;
	opts.room_id = room.room_id;

	m::feds::head(opts, [&room]
	(const auto &result)
	{
		handle_headfill(room, result);
		return true;
	});
}

void
ircd::m::fetch::handle_headfill(const m::room &room,
                                const m::feds::result &result)
try
{
	if(result.eptr)
		std::rethrow_exception(result.eptr);

	const json::array &prev_events
	{
		json::object(result.object["event"]).get("prev_events")
	};

	log::debug
	{
		log, "Got %zu heads for %s from '%s'",
		prev_events.size(),
		string_view{room.room_id},
		string_view{result.origin},
	};

	for(const json::string &event_id : prev_events)
		fetch::prefetch(room.room_id, event_id);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Requesting head events for %s from '%s' :%s",
		string_view{room.room_id},
		result.origin,
		e.what(),
	};
}

namespace ircd::m::fetch
{
	static void handle_backfill(const m::room &, const m::feds::result &);
}

void
IRCD_MODULE_EXPORT
ircd::m::fetch::backfill(const room &room)
{
	m::feds::opts opts;
	opts.room_id = room.room_id;
	opts.event_id = room.event_id;
	opts.argi[0] = 4;

	m::feds::backfill(opts, [&room]
	(const auto &result)
	{
		handle_backfill(room, result);
		return true;
	});
}

void
ircd::m::fetch::handle_backfill(const m::room &room,
                                const m::feds::result &result)
try
{
	if(result.eptr)
		std::rethrow_exception(result.eptr);

	const json::array &pdus
	{
		result.object["pdus"]
	};

	log::debug
	{
		log, "Got %zu events for %s off %s from '%s'",
		pdus.size(),
		string_view{room.room_id},
		string_view{room.event_id},
		string_view{result.origin},
	};

	for(const json::object &event : pdus)
	{
		const m::event::id &event_id
		{
			unquote(event.get("event_id"))
		};

		if(m::exists(event_id))
			continue;

		m::vm::opts vmopts;
		vmopts.nothrows = -1;
		m::vm::eval
		{
			m::event{event}, vmopts
		};
	}
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Requesting backfill for %s off %s from '%s' :%s",
		string_view{room.room_id},
		string_view{room.event_id},
		result.origin,
		e.what(),
	};
}

void
IRCD_MODULE_EXPORT
ircd::m::fetch::backfill(const room &room,
                         const net::hostport &remote)
{
	m::event::id::buf event_id;
	if(!room.event_id)
		event_id = m::v1::fetch_head(room.room_id, remote, room.any_user(my_host(), "join"));

	m::v1::backfill::opts opts;
	opts.remote = remote;
	opts.event_id = room.event_id?: event_id;
	opts.dynamic = true;
	const unique_buffer<mutable_buffer> buf
	{
		8_KiB
	};

	m::v1::backfill request
	{
		room.room_id, buf, std::move(opts)
	};

	request.wait(seconds(20)); //TODO: conf
	request.get();

    const json::object &response
    {
        request
    };

	log::debug
	{
		log, "Got %zu events for %s off %s from '%s'",
		json::array{response["pdus"]}.size(),
		string_view{room.room_id},
		string_view{room.event_id},
		host(remote),
	};

	for(const json::object &event : json::array(response["pdus"]))
	{
		const m::event::id &event_id
		{
			unquote(event.get("event_id"))
		};

		if(m::exists(event_id))
			continue;

		m::vm::opts vmopts;
		vmopts.nothrows = -1;
		m::vm::eval
		{
			m::event{event}, vmopts
		};
	}
}

namespace ircd::m::fetch
{
	static void handle_state_ids(const m::room &, const m::feds::result &);
}

void
IRCD_MODULE_EXPORT
ircd::m::fetch::state_ids(const room &room)
{
	m::feds::opts opts;
	opts.room_id = room.room_id;
	opts.event_id = room.event_id;
	opts.arg[0] = "ids";

	m::feds::state(opts, [&room]
	(const auto &result)
	{
		handle_state_ids(room, result);
		return true;
	});
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

	for(const json::string &event_id : ids)
		fetch::prefetch(room.room_id, event_id);
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
ircd::m::fetch::state_ids(const room &room,
                          const net::hostport &remote)
{
	m::v1::state::opts opts;
	opts.remote = remote;
	opts.event_id = room.event_id;
	opts.ids_only = true;
	opts.dynamic = true;
	const unique_buffer<mutable_buffer> buf
	{
		8_KiB
	};

	m::v1::state request
	{
		room.room_id, buf, std::move(opts)
	};

	request.wait(seconds(20)); //TODO: conf
	request.get();

    const json::object &response
    {
        request
    };

	const json::array &auth_chain_ids
	{
		response["auth_chain_ids"]
	};

	const json::array &pdu_ids
	{
		response["pdu_ids"]
	};

	for(const json::string &event_id : auth_chain_ids)
		fetch::prefetch(room.room_id, event_id);

	for(const json::string &event_id : pdu_ids)
		fetch::prefetch(room.room_id, event_id);
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
	vmopts.prev_check_exists = false;
	vmopts.warnlog |= m::vm::fault::STATE;
	vmopts.warnlog &= ~m::vm::fault::EXISTS;
	vmopts.errorlog &= ~m::vm::fault::STATE;
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

	start(room_id, event_id);
	return true;
}

void
IRCD_MODULE_EXPORT
ircd::m::fetch::start(const m::room::id &room_id,
                      const m::event::id &event_id)
{
	submit(event_id, room_id);
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

		request_cleanup();
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

void
ircd::m::fetch::request_cleanup()
{
	auto it(begin(requests));
	while(it != end(requests))
	{
		if(it->finished && empty(it->buf))
			it = requests.erase(it);
		else
			++it;
	}
}

void
ircd::m::fetch::request_handle()
{
	auto next
	{
		ctx::when_any(requests.begin(), requests.end())
	};

	if(!next.wait(seconds(5), std::nothrow))
	{
		for(const auto &request_ : requests)
		{
			auto &request(const_cast<fetch::request &>(request_));
			if(!request.finished)
				retry(request);
		}

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
	opts.remote = request.origin?: select_random_origin(request);
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
	static_cast<m::v1::event &>(request) =
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
	log::error
	{
		log, "Failed to start request for %s in %s to '%s' :%s",
		string_view{request.event_id},
		string_view{request.room_id},
		string_view{request.origin},
		e.what()
	};

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
