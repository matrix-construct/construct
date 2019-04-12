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

	if(opts.state_check_exists && !exists(room_id))
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
		{
			const m::room::auth auth{room};
			auth.chain_eval(auth, event_id.host());
		}
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

		if(exists(prev_id))
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
			start(prev_id, room_id);

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

void
IRCD_MODULE_EXPORT
ircd::m::fetch::state_ids(const room &room)
{
	m::feds::opts opts;
	opts.room_id = room.room_id;
	opts.event_id = room.event_id;
	opts.arg[0] = "ids";
	m::feds::state(opts, [&room](const auto &result)
	{
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
				if(!exists(m::event::id(event_id)))
					start(event_id, room.room_id);
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

		return true;
	});
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
		if(!exists(m::event::id(event_id)))
			start(event_id, room.room_id);

	for(const json::string &event_id : pdu_ids)
		if(!exists(m::event::id(event_id)))
			start(event_id, room.room_id);
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
// auth chain fetch
//

void
IRCD_MODULE_EXPORT
ircd::m::room::auth::chain_eval(const auth &auth,
                                const net::hostport &remote)
{
	m::vm::opts opts;
	opts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	opts.infolog_accept = true;
	opts.prev_check_exists = false;
	opts.warnlog |= m::vm::fault::STATE;
	opts.warnlog &= ~m::vm::fault::EXISTS;
	opts.errorlog &= ~m::vm::fault::STATE;

	chain_fetch(auth, remote, [&opts]
	(const json::object &event)
	{
		m::vm::eval
		{
			m::event{event}, opts
		};

		return true;
	});
}

bool
IRCD_MODULE_EXPORT
ircd::m::room::auth::chain_fetch(const auth &auth,
                                 const net::hostport &remote,
                                 const fetch_closure &closure)
{
	m::v1::event_auth::opts opts;
	opts.remote = remote;
	opts.dynamic = true;
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	m::v1::event_auth request
	{
		auth.room.room_id, auth.room.event_id, buf, std::move(opts)
	};

	request.wait(seconds(20)); //TODO: conf
	request.get();
    const json::array &auth_chain
    {
        request
    };

	std::vector<json::object> events(auth_chain.count());
	std::copy(begin(auth_chain), end(auth_chain), begin(events));
	std::sort(begin(events), end(events), []
	(const json::object &a, const json::object &b)
	{
		return a.at<uint64_t>("depth") < b.at<uint64_t>("depth");
	});

	for(const auto &event : events)
		if(!closure(event))
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
				return r.finished == 0;
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
	log::error
	{
		log, "fetch eval %s in %s :%s",
		string_view{it->event_id},
		string_view{it->room_id},
		e.what()
	};
}

//
// fetch::request
//

bool
IRCD_MODULE_EXPORT
ircd::m::fetch::request::prefetch(const m::room::id &room_id,
                                  const m::event::id &event_id)
{
	if(m::exists(event_id))
		return false;

	start(room_id, event_id);
	return true;
}

void
IRCD_MODULE_EXPORT
ircd::m::fetch::request::start(const m::room::id &room_id,
                               const m::event::id &event_id)
{
	fetch::start(event_id, room_id);
}

void
ircd::m::fetch::start(request &request)
{
	m::v1::event::opts opts;
	opts.dynamic = true;
	opts.remote = request.origin?: select_random_origin(request);
	start(request, opts);
}

void
ircd::m::fetch::start(request &request,
                      m::v1::event::opts &opts)
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

	if(!origins.random(closure, proffer))
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
