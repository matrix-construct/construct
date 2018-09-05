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

#include "vm_fetch.int.h"

//
// init
//

void
_init()
{
	m::vm::fetch::context = ctx::context
	{
		"fetcher",
		128_KiB,
		std::bind(&m::vm::fetch::worker),
		ctx::context::POST
	};

	log::debug
	{
		m::vm::log, "Fetch unit ready."
	};
}

void
_fini()
{
	log::debug
	{
		m::vm::log, "Shutting down fetch unit..."
	};

	m::vm::fetch::context.terminate();
	m::vm::fetch::context.join();

	log::debug
	{
		m::vm::log, "Fetch unit complete."
	};
}

//
// fetch_phase
//

decltype(m::vm::fetch::phase)
m::vm::fetch::phase
{
	"fetch", enter
};

void
m::vm::fetch::enter(eval &eval)
{
	assert(eval.event_);
	assert(eval.opts);

	const event::prev prev
	{
		*eval.event_
	};

	const auto &prev_events
	{
		json::get<"prev_events"_>(prev)
	};

	const size_t &prev_count
	{
		size(prev_events)
	};

	for(size_t i(0); i < prev_count; ++i)
	{
		const auto &prev_id{prev.prev_event(i)};
		if(!exists(prev_id))
		{
			log::warning
			{
				log, "Missing prev %s in %s in %s",
				string_view{prev_id},
				json::get<"event_id"_>(*eval.event_),
				json::get<"room_id"_>(*eval.event_)
			};
/*
			auto future
			{
				vm::evaluated(prev_id)
			};
*/
			//future.wait();
//			std::cout << "got " << prev_id << " for " << json::get<"event_id"_>(*eval.event_) << std::endl;
		}

		if(eval.opts->prev_check_exists && !exists(prev_id))
			throw error
			{
				fault::EVENT, "Missing prev event %s", prev_id
			};
	}
}

//
// API interface
//

json::object
m::vm::fetch::acquire(const m::room::id &room_id,
                      const m::event::id &event_id,
                      const mutable_buffer &buf)
{
	auto &request
	{
		_fetch(room_id, event_id)
	};

	request.dock.wait([&request]
	{
		return request.finished;
	});

	const unwind _remove_{[&event_id]
	{
		remove(event_id);
	}};

	if(request.eptr)
		std::rethrow_exception(request.eptr);

	const json::object &event
	{
		request
	};

	return json::object
	{
		data(buf), copy(buf, event)
	};
}

bool
m::vm::fetch::prefetch(const m::room::id &room_id,
                       const m::event::id &event_id)
{
	_fetch(room_id, event_id);
	return true;
}

bool
ircd::m::vm::fetch::remove(const m::event::id &event_id)
{
	const auto it
	{
		fetching.find(event_id)
	};

	if(it == end(fetching))
		return false;

	const request &r{*it};
	if(!r.dock.empty())
		return false;

	for(auto it(begin(fetched)); it != end(fetched); ++it)
		if(*it == &r)
		{
			fetched.erase(it);
			break;
		}

	fetching.erase(it);
	return true;
}

m::vm::fetch::request &
m::vm::fetch::_fetch(const m::room::id &room_id,
                     const m::event::id &event_id)
{
	auto it
	{
		fetching.lower_bound(event_id)
	};

	if(it == end(fetching) || it->event_id != event_id)
		it = fetching.emplace_hint(it, room_id, event_id);
	else
		return const_cast<struct request &>(*it);

	auto &request
	{
		const_cast<struct request &>(*it)
	};

	request.start();
	dock.notify_all();
	return request;
}

//
// fetcher
//

decltype(m::vm::fetch::dock)
m::vm::fetch::dock;

decltype(m::vm::fetch::fetching)
m::vm::fetch::fetching;

decltype(m::vm::fetch::fetched)
m::vm::fetch::fetched;

/// The fetch context is an internal worker which drives the fetch process
/// and then indicates completion to anybody waiting on a fetch. This involves
/// handling errors/timeouts from a fetch attempt and retrying with another
/// server etc.
///
decltype(m::vm::fetch::context)
m::vm::fetch::context;

void
ircd::m::vm::fetch::worker()
try
{
	while(1)
	{
		dock.wait(requesting);
		handle();
	}
}
catch(const std::exception &e)
{
	log::critical
	{
		"Fetch worker :%s", e.what()
	};
}

bool
ircd::m::vm::fetch::handle()
try
{
	auto next
	{
		ctx::when_any(begin(fetching), end(fetching))
	};

	if(!next.wait(seconds(2), std::nothrow))
		return true;

	const auto it
	{
		next.get()
	};

	if(it == end(fetching))
	{
		std::cout << "got nil" << std::endl;
		return false;
	}

	auto &request
	{
		const_cast<struct request &>(*it)
	};

	if(request.finished || request.eptr)
		return true;

	request.handle();
	return true;
}
catch(const std::exception &e)
{
	log::error
	{
		"Fetch worker :%s", e.what()
	};

	return true;
}

bool
ircd::m::vm::fetch::requesting()
{
	return std::any_of(begin(fetching), end(fetching), []
	(const request &request)
	{
		return !request.finished;
	});
}

//
// fetch::request
//

ircd::m::vm::fetch::request::request(const m::room::id &room_id,
                                     const m::event::id &event_id,
                                     const mutable_buffer &buf)
:room_id{room_id}
,event_id{event_id}
,_buf
{
	!buf?
		unique_buffer<mutable_buffer>{96_KiB}:
		unique_buffer<mutable_buffer>{}
}
,buf
{
	empty(buf)? _buf: buf
}
{
	// Ensure buffer has enough room for a worst-case event, request
	// headers and response headers.
	assert(size(this->buf) >= 64_KiB + 8_KiB + 8_KiB);
}

void
ircd::m::vm::fetch::request::start()
{
	m::v1::event::opts opts;
	opts.dynamic = false;
	opts.remote = origin?: select_random_origin();
	start(std::move(opts));
}

void
ircd::m::vm::fetch::request::start(m::v1::event::opts &&opts)
{
	if(!started)
		started = ircd::time();

	last = ircd::time();
	static_cast<m::v1::event &>(*this) =
	{
		this->event_id, this->buf, std::move(opts)
	};
}

ircd::string_view
ircd::m::vm::fetch::request::select_random_origin()
{
	const m::room::origins origins
	{
		room_id
	};

	// copies randomly selected origin into the attempted set.
	const auto closure{[this]
	(const string_view &origin)
	{
		this->select_origin(origin);
	}};

	// Tests if origin is potentially viable
	const auto proffer{[this]
	(const string_view &origin)
	{
		// Don't want to use a peer we already tried and failed with.
		if(attempted.count(origin))
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
			string_view{event_id},
			string_view{room_id},
		};

	return this->origin;
}

ircd::string_view
ircd::m::vm::fetch::request::select_origin(const string_view &origin)
{
	const auto iit
	{
		attempted.emplace(std::string{origin})
	};

	this->origin = *iit.first;
	return this->origin;
}

void
ircd::m::vm::fetch::request::handle()
{
	auto &future
	{
		static_cast<m::v1::event &>(*this)
	};

	future.wait(); try
	{
		future.get();
	}
	catch(...)
	{
		eptr = std::current_exception();
	}

	if(!eptr)
		finish();
	else
		retry();
}

void
ircd::m::vm::fetch::request::retry()
try
{
	eptr = std::exception_ptr{};
	origin = {};
	start();
}
catch(...)
{
	eptr = std::current_exception();
	finish();
}

void
ircd::m::vm::fetch::request::finish()
{
	finished = ircd::time();
	fetched.emplace_back(this);
	dock.notify_all();
}

bool
ircd::m::vm::fetch::request::operator()(const request &a,
                                        const request &b)
const
{
	return a.event_id < b.event_id;
}

bool
ircd::m::vm::fetch::request::operator()(const request &a,
                                        const string_view &b)
const
{
	return a.event_id < b;
}

bool
ircd::m::vm::fetch::request::operator()(const string_view &a,
                                        const request &b)
const
{
	return a < b.event_id;
}

//
//
//

extern "C" void
auth_chain_fetch(const m::room::id &room_id,
                 const m::event::id &event_id,
                 const net::hostport &remote,
                 const milliseconds &timeout,
                 const std::function<bool (const m::event &)> &closure)
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
		room_id, event_id, buf, std::move(opts)
	};

	request.wait(timeout);
	request.get();

    const json::array &auth_chain
    {
        request
    };

	std::vector<m::event> events{auth_chain.count()};
	std::transform(begin(auth_chain), end(auth_chain), begin(events), []
	(const json::object &pdu)
	{
		return m::event{pdu};
	});

	std::sort(begin(events), end(events));
	for(const auto &event : events)
		if(!closure(event))
			return;
}

extern "C" void
auth_chain_eval(const m::room::id &room_id,
                const m::event::id &event_id,
                const net::hostport &remote)
{
	m::vm::opts opts;
	opts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	opts.non_conform.set(m::event::conforms::MISSING_MEMBERSHIP);
	opts.infolog_accept = true;
	opts.warnlog |= m::vm::fault::STATE;
	opts.warnlog &= ~m::vm::fault::EXISTS;
	opts.errorlog &= ~m::vm::fault::STATE;

	auth_chain_fetch(room_id, event_id, remote, seconds(30), [&opts]
	(const auto &event)
	{
		m::vm::eval
		{
			event, opts
		};

		return true;
	});
}
