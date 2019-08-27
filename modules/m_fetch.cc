// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::fetch
{
	static bool operator<(const request &a, const request &b) noexcept;
	static bool operator<(const request &a, const string_view &b) noexcept;
	static bool operator<(const string_view &a, const request &b) noexcept;

	extern ctx::dock dock;
	extern ctx::mutex requests_mutex;
	extern std::set<request, std::less<>> requests;
	extern std::multimap<room::id, request *> rooms;
	extern ctx::context request_context;
	extern conf::item<size_t> requests_max;
	extern conf::item<seconds> auth_timeout;
	extern conf::item<seconds> timeout;
	extern conf::item<bool> enable;
	extern log::log log;

	static bool timedout(const request &, const time_t &now);
	static string_view select_origin(request &, const string_view &);
	static string_view select_random_origin(request &);
	static void finish(request &);
	static void retry(request &);
	static bool start(request &, m::v1::event::opts &);
	static bool start(request &);
	static bool handle(request &);

	static void request_handle(const decltype(requests)::iterator &);
	static void request_handle();
	static size_t request_cleanup();
	static void request_worker();

	template<class... args>
	static ctx::future<result>
	submit(const event::id &,
	       const room::id &,
	       const size_t &bufsz = 8_KiB,
	       args&&...);

	static void init();
	static void fini();
}

ircd::mapi::header
IRCD_MODULE
{
    "Event Fetch Unit", ircd::m::fetch::init, ircd::m::fetch::fini
};

decltype(ircd::m::fetch::log)
ircd::m::fetch::log
{
	"m.fetch"
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

decltype(ircd::m::fetch::request_context)
ircd::m::fetch::request_context
{
	"m.fetch.req", 512_KiB, &request_worker, context::POST
};

decltype(ircd::m::fetch::rooms)
ircd::m::fetch::rooms;

decltype(ircd::m::fetch::requests)
ircd::m::fetch::requests;

decltype(ircd::m::fetch::requests_mutex)
ircd::m::fetch::requests_mutex;

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
	request_context.join();
	requests.clear();

	assert(requests.empty());
}

///////////////////////////////////////////////////////////////////////////////
//
// m/fetch.h
//

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
		16_KiB
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

ircd::ctx::future<ircd::m::fetch::result>
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

	dock.wait([]
	{
		return count() < size_t(requests_max);
	});

	return submit(event_id, room_id);
}

size_t
IRCD_MODULE_EXPORT
ircd::m::fetch::count()
{
	return requests.size();
}

bool
IRCD_MODULE_EXPORT
ircd::m::fetch::exists(const m::event::id &event_id)
{
	return requests.count(string_view{event_id});
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
ircd::ctx::future<ircd::m::fetch::result>
ircd::m::fetch::submit(const m::event::id &event_id,
                       const m::room::id &room_id,
                       const size_t &bufsz,
                       args&&... a)
{
	assert(room_id && event_id);
	std::unique_lock lock
	{
		requests_mutex
	};

	const scope_notify dock
	{
		fetch::dock
	};

	auto it(requests.lower_bound(string_view(event_id)));
	if(it != end(requests) && it->event_id == event_id)
	{
		assert(it->room_id == room_id);
		return ctx::future<result>{}; //TODO: shared_future.
	}

	it = requests.emplace_hint(it, room_id, event_id, bufsz, std::forward<args>(a)...);
	auto &request
	{
		const_cast<fetch::request &>(*it)
	};

	ctx::future<result> ret
	{
		request.promise
	};

	start(request);
	return ret;
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
			(const auto &request)
			{
				return request.started || request.finished;
			});
		});

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
ircd::m::fetch::request_handle()
{
	std::unique_lock lock
	{
		requests_mutex
	};

	if(requests.empty())
		return;

	auto next
	{
		ctx::when_any(requests.begin(), requests.end())
	};

	bool timeout{true};
	{
		const unlock_guard unlock{lock};
		timeout = !next.wait(seconds(timeout), std::nothrow);
	};

	if(timeout)
	{
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
{
	auto &request
	{
		const_cast<fetch::request &>(*it)
	};

	if(!request.finished && !handle(request))
		return;

	requests.erase(it);
}

size_t
ircd::m::fetch::request_cleanup()
{
	size_t ret(0);
	const auto now(ircd::time());
	for(auto it(begin(requests)); it != end(requests); ++it)
	{
		auto &request
		{
			const_cast<fetch::request &>(*it)
		};

		if(!request.started)
		{
			start(request);
			continue;
		}

		if(!request.finished && timedout(request, now))
			retry(request);
	}

	for(auto it(begin(requests)); it != end(requests); )
	{
		auto &request
		{
			const_cast<fetch::request &>(*it)
		};

		if(request.finished)
		{
			it = requests.erase(it);
			++ret;
		}
		else ++it;
	}

	return ret;
}

//
// fetch::request
//

bool
ircd::m::fetch::start(request &request) try
{
	m::v1::event::opts opts;
	opts.dynamic = true;

	assert(request.finished == 0);
	if(!request.started)
		request.started = ircd::time();

	if(!request.origin)
	{
		select_random_origin(request);
		opts.remote = request.origin;
	}

	while(request.origin)
	{
		if(start(request, opts))
			return true;

		select_random_origin(request);
		opts.remote = request.origin;
	}

	assert(!request.finished);
	finish(request);
	return false;
}
catch(...)
{
	assert(!request.finished);
	request.eptr = std::current_exception();
	finish(request);
	return false;
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
		server::cancel(request);
		throw;
	}

	log::debug
	{
		log, "Starting request for %s in %s from '%s'",
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
		"Starting request for %s in %s to '%s' :%s %s",
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
		"Starting request for %s in %s to '%s' :%s",
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
			log, "Received %u %s good %s in %s from '%s'",
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
			log, "Erroneous remote for %s in %s from '%s' :%s",
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

	#if 0
	log::logf
	{
		log, request.eptr? log::DERROR: log::DEBUG,
		"%s in %s started:%ld finished:%d attempted:%zu abandon:%b %S%s",
		string_view{request.event_id},
		string_view{request.room_id},
		request.started,
		request.finished,
		request.attempted.size(),
		!request.promise,
		request.eptr? " :" : "",
		what(request.eptr),
	};
	#endif

	if(!request.promise)
		return;

	if(request.eptr)
	{
		request.promise.set_exception(std::move(request.eptr));
		return;
	}

	request.promise.set_value(result{request});
}

bool
ircd::m::fetch::timedout(const request &request,
                         const time_t &now)
{
	assert(request.started && request.finished >= 0 && request.last != 0);
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

//
// result::result
//

ircd::m::fetch::result::result(request &request)
:m::event
{
	static_cast<json::object>(request)["event"]
}
,buf
{
	std::move(request.in.dynamic)
}
{
}
