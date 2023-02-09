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
	static bool operator==(const opts &a, const opts &b) noexcept;
	static bool operator==(const request &a, const opts &b) noexcept;
	static bool operator==(const opts &a, const request &b) noexcept;
	static bool operator==(const request &a, const request &b) noexcept;
	static bool operator<(const opts &a, const opts &b) noexcept;
	static bool operator<(const request &a, const opts &b) noexcept;
	static bool operator<(const opts &a, const request &b) noexcept;
	static bool operator<(const request &a, const request &b) noexcept;

	extern ctx::dock dock;
	extern ctx::mutex requests_mutex;
	extern std::set<request, std::less<>> requests;
	extern ctx::context request_context;
	extern conf::item<size_t> backfill_limit_default;
	extern conf::item<size_t> requests_max;
	extern conf::item<seconds> timeout;
	extern conf::item<bool> enable;
	extern log::log log;

	static bool timedout(const request &, const system_point &now);
	extern void check_response(const request &, const json::object &);
	static bool proffer_remote(request &, const string_view &);
	static bool select_remote(request &, const string_view &);
	static bool select_random_remote(request &);
	static void finish(request &);
	static void retry(request &);
	static bool start(request &, const string_view &remote);
	static bool start(request &);
	static void handle_result(request &);
	static bool handle(request &);

	static bool request_handle(const decltype(requests)::iterator &);
	static void request_handle();
	static size_t request_cleanup();
	static void request_worker();
}

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

decltype(ircd::m::fetch::requests_max)
ircd::m::fetch::requests_max
{
	{ "name",     "ircd.m.fetch.requests.max" },
	{ "default",  2048L                       },
};

decltype(ircd::m::fetch::backfill_limit_default)
ircd::m::fetch::backfill_limit_default
{
	{ "name",     "ircd.m.fetch.backfill.limit.default" },
	{ "default",  96L                                   },
};

decltype(ircd::m::fetch::dock)
ircd::m::fetch::dock;

decltype(ircd::m::fetch::requests)
ircd::m::fetch::requests;

decltype(ircd::m::fetch::requests_mutex)
ircd::m::fetch::requests_mutex;

decltype(ircd::m::fetch::request_context)
ircd::m::fetch::request_context
{
	"m.fetch.req",
	1_MiB,
	&request_worker,
	context::POST
};

//
// init
//

ircd::m::fetch::init::init()
{
	assert(requests.empty());
}

ircd::m::fetch::init::~init()
noexcept
{
	request_context.terminate();
	request_context.join();
	requests.clear();
	assert(requests.empty());
}

//
// <ircd/m/fetch.h>
//

ircd::ctx::future<ircd::m::fetch::result>
ircd::m::fetch::start(opts opts)
{
	assert(opts.room_id && opts.event_id);
	if(unlikely(run::level == run::level::QUIT))
		throw m::UNAVAILABLE
		{
			"Cannot start fetch requests at this time."
		};

	// in case requests are started before runlevel RUN they are stalled here
	run::barrier<m::UNAVAILABLE>
	{
		"The fetch unit is unavailable to start requests."
	};

	// in case the fetch unit has reached capacity the context will yield.
	dock.wait([]
	{
		return count() < size_t(requests_max);
	});

	// the state in this unit is primarily driven by the fetch::context; in
	// some cases it may want to prevent others from changing this state until
	// it's ready to accept.
	std::unique_lock lock
	{
		requests_mutex
	};

	const scope_notify dock
	{
		fetch::dock
	};

	auto it
	{
		requests.lower_bound(opts)
	};

	const bool exists
	{
		it != end(requests) && *it == opts
	};

	assert(!exists || it->opts.room_id == opts.room_id);
	if(!exists)
		it = requests.emplace_hint(it, std::move(opts));

	auto &request
	{
		const_cast<fetch::request &>(*it)
	};

	ctx::future<result> ret
	{
		request.promise
	};

	if(!exists)
		start(request);

	return ret;
}

size_t
ircd::m::fetch::count()
{
	return requests.size();
}

bool
ircd::m::fetch::exists(const opts &opts)
{
	return requests.count(opts);
}

bool
ircd::m::fetch::for_each(const std::function<bool (request &)> &closure)
{
	for(auto &request : requests)
		if(!closure(const_cast<fetch::request &>(request)))
			return false;

	return true;
}

ircd::string_view
ircd::m::fetch::reflect(const op &op)
{
	switch(op)
	{
		case op::noop:        return "noop";
		case op::auth:        return "auth";
		case op::event:       return "event";
		case op::backfill:    return "backfill";
	}

	return "?????";
}

//
// internal
//

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
				return !request.finished;
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

	const scope_notify notify
	{
		fetch::dock
	};

	static const auto dereferencer{[]
	(auto &it) -> server::request &
	{
		// If the request doesn't have a server::request future attached
		// during this pass we reference this default constructed static
		// instance which when_any() will treat as a no-op.
		static server::request request_skip;
		auto &request(mutable_cast(*it));
		return request.future?
			*request.future:
			request_skip;
	}};

	auto next
	{
		ctx::when_any(requests.begin(), requests.end(), dereferencer)
	};

	bool timedout{true};
	{
		const unlock_guard unlock
		{
			lock
		};

		timedout = !next.wait(seconds(timeout), std::nothrow);
	};

	if(likely(!timedout))
	{
		const auto it
		{
			next.get()
		};

		if(it != end(requests))
			if(!request_handle(it))
				return;
	}

	request_cleanup();
}

bool
ircd::m::fetch::request_handle(const decltype(requests)::iterator &it)
{
	auto &request
	{
		mutable_cast(*it)
	};

	if(!request.finished)
		if(!handle(request))
			return false;

	requests.erase(it);
	return true;
}

size_t
ircd::m::fetch::request_cleanup()
try
{
	const auto now
	{
		ircd::now<system_point>()
	};

	size_t ret(0);
	for(auto it(begin(requests)); it != end(requests); ++it)
	{
		auto &request(mutable_cast(*it));
		if(!!request.finished || !request.promise)
			continue;

		else if(!request.started)
			start(request);

		else if(timedout(request, now))
			retry(request);
	}

	auto it(begin(requests)); while(it != end(requests))
	{
		auto &request(mutable_cast(*it));
		if(!!request.finished || !request.promise)
		{
			it = requests.erase(it);
			++ret;
		}
		else ++it;
	}

	return ret;
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "request_cleanup(): %s",
		e.what(),
	};

	throw;
}

//
// fetch::request
//

bool
ircd::m::fetch::start(request &request)
try
{
	assert(request.promise);
	assert(!request.finished);

	// Attempt the user's hint first
	if(!request.started && !request.origin && request.opts.hint)
		if(proffer_remote(request, request.opts.hint))
			select_remote(request, request.opts.hint);

	// When no user hint, use legacy event_id hostpart as hint.
	if(!request.started && !request.origin)
		if(proffer_remote(request, request.opts.event_id.host()))
			select_remote(request, request.opts.event_id.host());

	if(!!request.started)
		if(!request.opts.attempt_limit || request.attempted.size() < request.opts.attempt_limit)
			select_random_remote(request);

	if(!request.started && !request.origin)
		select_random_remote(request);

	if(!request.started)
		request.started = ircd::now<system_point>();

	while(request.origin)
	{
		if(unlikely(!request.promise))
			throw ctx::broken_promise
			{
				"Fetch interrupted."
			};

		if(start(request, request.origin))
			return true;

		if(request.opts.attempt_limit)
			if(request.attempted.size() >= request.opts.attempt_limit)
				break;

		select_random_remote(request);
	}

	throw m::NOT_FOUND
	{
		"Cannot find any server to fetch %s in %s in %zu attempts",
		string_view{request.opts.event_id},
		string_view{request.opts.room_id},
		request.attempted.size(),
	};
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
                      const string_view &remote)
try
{
	if(unlikely(run::level != run::level::RUN))
		throw m::UNAVAILABLE
		{
			"Cannot start fetch requests in runlevel."
		};

	assert(request.promise);
	assert(!request.finished);
	request.last = ircd::now<system_point>();
	if(!request.started)
		request.started = request.last;

	switch(request.opts.op)
	{
		case op::noop:
			break;

		case op::auth:
		{
			fed::event_auth::opts opts;
			opts.remote = remote;
			request.future = std::make_unique<fed::event_auth>
			(
				request.opts.room_id,
				request.opts.event_id,
				request.buf,
				std::move(opts)
			);

			break;
		}

		case op::event:
		{
			fed::event::opts opts;
			opts.remote = remote;
			request.future = std::make_unique<fed::event>
			(
				request.opts.event_id,
				request.buf,
				std::move(opts)
			);

			break;
		}

		case op::backfill:
		{
			fed::backfill::opts opts;
			opts.remote = remote;
			opts.limit = request.opts.backfill_limit;
			opts.limit = opts.limit?: size_t(backfill_limit_default);
			opts.event_id = request.opts.event_id;
			request.future = std::make_unique<fed::backfill>
			(
				request.opts.room_id,
				request.buf,
				std::move(opts)
			);

			break;
		}
	}

	log::debug
	{
		log, "Starting %s request for %s in %s from '%s'",
		reflect(request.opts.op),
		string_view{request.opts.event_id},
		string_view{request.opts.room_id},
		string_view{request.origin},
	};

	dock.notify_all();
	return true;
}
catch(const m::UNAVAILABLE &e)
{
	throw;
}
catch(const ctx::interrupted &e)
{
	throw;
}
catch(const http::error &e)
{
	log::derror
	{
		log, "Starting %s request for %s in %s to '%s' :%s %s",
		reflect(request.opts.op),
		string_view{request.opts.event_id},
		string_view{request.opts.room_id},
		string_view{request.origin},
		e.what(),
		e.content,
	};

	return false;
}
catch(const server::error &e)
{
	log::derror
	{
		log, "Starting %s request for %s in %s to '%s' :%s",
		reflect(request.opts.op),
		string_view{request.opts.event_id},
		string_view{request.opts.room_id},
		string_view{request.origin},
		e.what(),
	};

	return false;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Starting %s request for %s in %s to '%s' :%s",
		reflect(request.opts.op),
		string_view{request.opts.event_id},
		string_view{request.opts.room_id},
		string_view{request.origin},
		e.what()
	};

	return false;
}

bool
ircd::m::fetch::select_random_remote(request &request)
{
	// Tests if remote is potentially viable
	const auto proffer{[&request](const string_view &remote)
	{
		return proffer_remote(request, remote);
	}};

	// copies randomly selected remote into the attempted set.
	const auto closure{[&request](const string_view &remote)
	{
		select_remote(request, remote);
	}};

	const m::room::origins origins
	{
		request.opts.room_id
	};

	// Select a random server in the room
	request.origin = {};
	if(origins.random(closure, proffer))
		return true;

	// If nothing found attempt hosts from mxids
	const string_view mxid[2]
	{
		request.opts.event_id.host(),
		request.opts.room_id.host(),
	};

	for(const auto &remote : mxid)
		if(proffer_remote(request, remote))
			if(select_remote(request, remote))
				return true;

	return false;
}

bool
ircd::m::fetch::select_remote(request &request,
                              const string_view &remote)
{
	const auto it
	{
		request.attempted.lower_bound(remote)
	};

	if(it != request.attempted.end() && *it == remote)
		return false;

	const auto iit
	{
		request.attempted.emplace_hint(it, remote)
	};

	request.origin = *iit;
	return true;
}

bool
ircd::m::fetch::proffer_remote(request &request,
                               const string_view &remote)
{
	// Case sentinel false.
	if(!remote)
		return false;

	// Don't want to request from myself.
	if(my_host(remote))
		return false;

	// Don't want to use a peer we already tried and failed with.
	if(request.attempted.count(remote))
		return false;

	// Don't want to use a peer marked with an error by ircd::server
	if(fed::errant(remote))
		return false;

	return true;
}

bool
ircd::m::fetch::handle(request &request)
{
	if(likely(request.future))
		handle_result(request);

	if(!request.eptr || !request.promise)
		finish(request);
	else
		retry(request);

	return !!request.finished;
}

void
ircd::m::fetch::handle_result(request &request)
try
{
	assert(request.future);
	const auto code
	{
		request.future->get()
	};

	const string_view content
	{
		request.future->in.content
	};

	check_response(request, content);
	if(unlikely(!request.promise))
		throw ctx::broken_promise
		{
			"Fetch response check interrupted."
		};

	assert(request.promise);
	char pbuf[48];
	log::debug
	{
		log, "Received %u %s good %s %s in %s from '%s' %s",
		uint(code),
		status(code),
		reflect(request.opts.op),
		string_view{request.opts.event_id},
		string_view{request.opts.room_id},
		string_view{request.origin},
		pretty(pbuf, iec(size(content))),
	};
}
catch(...)
{
	request.eptr = std::current_exception();

	log::derror
	{
		log, "%s error for %s %s in %s :%s",
		string_view{request.origin},
		reflect(request.opts.op),
		string_view{request.opts.event_id},
		string_view{request.opts.room_id},
		what(request.eptr),
	};
}

void
ircd::m::fetch::retry(request &request)
try
{
	assert(request.promise);
	assert(!request.finished);
	assert(!!request.started && !!request.last);

	if(request.future)
	{
		server::cancel(*request.future);
		request.future.reset(nullptr);
	}

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
	request.finished = ircd::now<system_point>();

	if constexpr((false))
		log::logf
		{
			log, request.eptr? log::DERROR: log::DEBUG,
			"Finished %s in %s started:%ld finished:%d attempted:%zu abandon:%b %s%s",
			string_view{request.opts.event_id},
			string_view{request.opts.room_id},
			duration_cast<seconds>(tse(request.started)).count(),
			duration_cast<seconds>(tse(request.finished)).count(),
			request.attempted.size(),
			!request.promise,
			request.eptr? " :" : "",
			what(request.eptr),
		};

	if(!request.promise)
		return;

	if(request.eptr)
	{
		request.promise.set_exception(std::move(request.eptr));
		return;
	}

	result res;
	res.buf = std::move(request.future->in.dynamic);
	res.content = res.buf;
	strlcpy(res.origin, request.origin);
	assert(request.origin == res.origin);
	request.promise.set_value(std::move(res));
}

bool
ircd::m::fetch::timedout(const request &request,
                         const system_point &now)
{
	assert(!!request.started && !request.finished && !!request.last);
	return request.last + seconds(timeout) < now;
}

bool
ircd::m::fetch::operator<(const request &a, const request &b)
noexcept
{
	return a.opts < b.opts;
}

bool
ircd::m::fetch::operator<(const opts &a, const request &b)
noexcept
{
	return a < b.opts;
}

bool
ircd::m::fetch::operator<(const request &a, const opts &b)
noexcept
{
	return a.opts < b;
}

bool
ircd::m::fetch::operator<(const opts &a, const opts &b)
noexcept
{
	if(uint(a.op) < uint(b.op))
		return true;

	else if(uint(a.op) > uint(b.op))
		return false;

	else if(a.room_id < b.room_id)
		return true;

	else if(a.room_id > b.room_id)
		return false;

	else if(a.event_id < b.event_id)
		return true;

	else if(a.event_id > b.event_id)
		return false;

	assert(operator==(a, b));
	return false;
}

bool
ircd::m::fetch::operator==(const request &a, const request &b)
noexcept
{
	return a.opts == b.opts;
}

bool
ircd::m::fetch::operator==(const opts &a, const request &b)
noexcept
{
	return a == b.opts;
}

bool
ircd::m::fetch::operator==(const request &a, const opts &b)
noexcept
{
	return a.opts == b;
}

bool
ircd::m::fetch::operator==(const opts &a, const opts &b)
noexcept
{
	return uint(a.op) == uint(b.op) &&
	       a.event_id == b.event_id &&
	       a.room_id == b.room_id;
}

//
// request::request
//

ircd::m::fetch::request::request(fetch::opts opts)
:opts
{
	std::move(opts)
}
,buf
{
	this->opts.bufsz?: 16_KiB
}
,event_id
{
	this->opts.event_id
}
,room_id
{
	this->opts.room_id
}
{
	this->opts.event_id = this->event_id;
	this->opts.room_id = this->room_id;
}

ircd::m::fetch::request::~request()
noexcept
{
	//TODO: bad things unless this first here
	future.reset(nullptr);
}
