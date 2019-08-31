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
	extern conf::item<size_t> requests_max;
	extern conf::item<seconds> timeout;
	extern conf::item<bool> enable;
	extern log::log log;

	static bool timedout(const request &, const system_point &now);
	static void _check_event(const request &, const m::event &);
	static void check_response(const request &, const json::object &);
	static string_view select_origin(request &, const string_view &);
	static string_view select_random_origin(request &);
	static void finish(request &);
	static void retry(request &);
	static bool start(request &, const string_view &remote);
	static bool start(request &);
	static void handle_result(request &);
	static bool handle(request &);

	static void request_handle(const decltype(requests)::iterator &);
	static void request_handle();
	static size_t request_cleanup();
	static void request_worker();

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

//
// <ircd/m/fetch.h>
//

static void
wait_for_run()
{
	using namespace ircd::run;
	using namespace ircd;

	changed::dock.wait([]
	{
		return level == level::RUN || level == level::QUIT;
	});

	if(unlikely(level != level::RUN))
		throw m::UNAVAILABLE
		{
			"Cannot fetch in runlevel '%s'",
			reflect(level)
		};
}

ircd::ctx::future<ircd::m::fetch::result>
IRCD_MODULE_EXPORT
ircd::m::fetch::start(opts opts)
{
	assert(opts.room_id && opts.event_id);

	// in case requests are started before runlevel RUN they are stalled here
	wait_for_run();

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

	if(it != end(requests) && !(*it < opts) && !(opts < *it))
	{
		assert(it->opts.room_id == opts.room_id);
		return ctx::future<result>{}; //TODO: shared_future.
	}

	it = requests.emplace_hint(it, opts);
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

size_t
IRCD_MODULE_EXPORT
ircd::m::fetch::count()
{
	return requests.size();
}

bool
IRCD_MODULE_EXPORT
ircd::m::fetch::exists(const opts &opts)
{
	return requests.count(opts);
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

ircd::string_view
IRCD_MODULE_EXPORT
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

	const unwind cleanup
	{
		request_cleanup
	};

	static const auto dereferencer{[]
	(auto &it) -> server::request &
	{
		auto &request
		{
			const_cast<fetch::request &>(*it)
		};

		assert(request.future);
		return *request.future;
	}};

	auto next
	{
		ctx::when_any(requests.begin(), requests.end(), dereferencer)
	};

	bool timeout{true};
	{
		const unlock_guard unlock
		{
			lock
		};

		timeout = !next.wait(seconds(timeout), std::nothrow);
	};

	if(unlikely(timeout))
		return;

	const auto it
	{
		next.get()
	};

	if(unlikely(it == end(requests)))
		return;

	request_handle(it);
}

void
ircd::m::fetch::request_handle(const decltype(requests)::iterator &it)
{
	auto &request
	{
		const_cast<fetch::request &>(*it)
	};

	if(!request.finished)
		if(!handle(request))
			return;

	requests.erase(it);
}

size_t
ircd::m::fetch::request_cleanup()
{
	const auto now
	{
		ircd::now<system_point>()
	};

	size_t ret(0);
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

	auto it(begin(requests)); while(it != end(requests))
	{
		auto &request
		{
			const_cast<fetch::request &>(*it)
		};

		if(!!request.finished)
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
	assert(!request.finished);
	if(!request.started && !request.origin)
		request.origin = request.opts.hint;

	if(!request.started)
		request.started = ircd::now<system_point>();

	if(!request.origin)
		select_random_origin(request);

	while(request.origin)
	{
		if(start(request, request.origin))
			return true;

		select_random_origin(request);
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
                      const string_view &remote)
try
{
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
			v1::event_auth::opts opts;
			opts.remote = remote;
			opts.dynamic = true;
			request.future = std::make_unique<v1::event_auth>
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
			v1::event::opts opts;
			opts.remote = remote;
			opts.dynamic = true;
			request.future = std::make_unique<v1::event>
			(
				request.opts.event_id,
				request.buf,
				std::move(opts)
			);

			break;
		}

		case op::backfill:
		{
			v1::backfill::opts opts;
			opts.remote = remote;
			opts.dynamic = true;
			opts.limit = request.opts.limit?: 64;
			opts.event_id = request.opts.event_id;
			request.future = std::make_unique<v1::backfill>
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
catch(const http::error &e)
{
	log::logf
	{
		log, run::level == run::level::QUIT? log::DERROR: log::ERROR,
		"Starting %s request for %s in %s to '%s' :%s %s",
		reflect(request.opts.op),
		string_view{request.opts.event_id},
		string_view{request.opts.room_id},
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
		"Starting %s request for %s in %s to '%s' :%s",
		reflect(request.opts.op),
		string_view{request.opts.event_id},
		string_view{request.opts.room_id},
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
		request.opts.room_id
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
			string_view{request.opts.event_id},
			string_view{request.opts.room_id},
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
	if(likely(request.future))
		handle_result(request);

	if(!request.eptr)
		finish(request);
	else
		retry(request);

	return !!request.finished;
}

void
ircd::m::fetch::handle_result(request &request)
try
{
	const auto code
	{
		request.future->get()
	};

	const string_view &content
	{
		request.future->in.content
	};

	check_response(request, content);

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
		log, "Erroneous remote for %s %s in %s from '%s' :%s",
		reflect(request.opts.op),
		string_view{request.opts.event_id},
		string_view{request.opts.room_id},
		string_view{request.origin},
		what(request.eptr),
	};
}

void
ircd::m::fetch::retry(request &request)
try
{
	assert(!request.finished);
	assert(!!request.started && !!request.last);

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

	#if 0
	log::logf
	{
		log, request.eptr? log::DERROR: log::DEBUG,
		"%s in %s started:%ld finished:%d attempted:%zu abandon:%b %S%s",
		string_view{request.opts.event_id},
		string_view{request.opts.room_id},
		seconds(tse(request.started)).count(),
		seconds(tse(request.finished)).count(),
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

	result res;
	res.buf = std::move(request.future->in.dynamic);
	res.content = res.buf;
	request.promise.set_value(std::move(res));
}

namespace ircd::m::fetch
{
	static void check_response__backfill(const request &, const json::object &);
	static void check_response__event(const request &, const json::object &);
	static void check_response__auth(const request &, const json::object &);

	extern conf::item<bool> check_event_id;
	extern conf::item<bool> check_conforms;
	extern conf::item<int> check_signature;
}

decltype(ircd::m::fetch::check_event_id)
ircd::m::fetch::check_event_id
{
	{ "name",     "ircd.m.fetch.check.event_id" },
	{ "default",  true                          },
};

decltype(ircd::m::fetch::check_conforms)
ircd::m::fetch::check_conforms
{
	{ "name",     "ircd.m.fetch.check.conforms" },
	{ "default",  false                         },
};

decltype(ircd::m::fetch::check_signature)
ircd::m::fetch::check_signature
{
	{ "name",         "ircd.m.fetch.check.signature" },
	{ "default",      true                           },
	{ "description",

	R"(
	false - Signatures of events will not be checked by the fetch unit (they
	are still checked normally during evaluation; this conf item does not
	disable event signature verification for the server).

	true - Signatures of events will be checked by the fetch unit such that
	bogus responses allow the fetcher to try the next server. This check might
	not occur in all cases. It will only occur if the server has the public
	key already; fetch unit worker contexts cannot be blocked trying to obtain
	unknown keys from remote hosts.
	)"},
};

void
ircd::m::fetch::check_response(const request &request,
                               const json::object &response)
{
	switch(request.opts.op)
	{
		case op::backfill:
			return check_response__backfill(request, response);

		case op::event:
			return check_response__event(request, response);

		case op::auth:
			return check_response__auth(request, response);

		case op::noop:
			return;
	}

	ircd::panic
	{
		"Unchecked response; fetch op:%u",
		uint(request.opts.op),
	};
}

void
ircd::m::fetch::check_response__auth(const request &request,
                                     const json::object &response)
{
	const json::array &auth_chain
	{
		response.at("auth_chain")
	};

	for(const json::object &auth_event : auth_chain)
	{
		event::id::buf event_id;
		const m::event event
		{
			event_id, auth_event
		};

		_check_event(request, event);
	}
}

void
ircd::m::fetch::check_response__event(const request &request,
                                      const json::object &response)
{
	const json::array &pdus
	{
		response.at("pdus")
	};

	const m::event event
	{
		pdus.at(0), request.opts.event_id
	};

	_check_event(request, event);
}

void
ircd::m::fetch::check_response__backfill(const request &request,
                                         const json::object &response)
{
	const json::array &pdus
	{
		response.at("pdus")
	};

	for(const json::object &event : pdus)
	{
		event::id::buf event_id;
		const m::event _event
		{
			event_id, event
		};

		_check_event(request, _event);
	}
}

void
ircd::m::fetch::_check_event(const request &request,
                             const m::event &event)
{
	if(check_event_id && !m::check_id(event))
	{
		event::id::buf buf;
		const m::event &claim
		{
			buf, event.source
		};

		throw ircd::error
		{
			"event::id claim:%s != sought:%s",
			string_view{claim.event_id},
			string_view{request.opts.event_id},
		};
	}

	if(check_conforms)
	{
		thread_local char buf[128];
		const m::event::conforms conforms
		{
			event
		};

		const string_view failures
		{
			conforms.string(buf)
		};

		assert(failures || conforms.clean());
		if(!conforms.clean())
			throw ircd::error
			{
				"Non-conforming event in response :%s",
				failures,
			};
	}

	if(check_signature)
	{
		const string_view &server
		{
			!json::get<"origin"_>(event)?
				user::id(at<"sender"_>(event)).host():
				string_view(json::get<"origin"_>(event))
		};

		const json::object &signatures
		{
			at<"signatures"_>(event).at("server")
		};

		const json::string &key_id
		{
			!signatures.empty()?
				signatures.begin()->first:
				string_view{},
		};

		if(!key_id)
			throw ircd::error
			{
				"Cannot find any keys for '%s' in event.signatures",
				server,
			};

		if(m::keys::cache::has(server, key_id))
			if(!verify(event, server))
				throw ircd::error
				{
					"Signature verification failed."
				};
	}
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
	return uint(a.op) < uint(b.op) ||
	       a.event_id < b.event_id ||
	       a.room_id < b.room_id;
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

ircd::m::fetch::request::request(const fetch::opts &opts)
:opts
{
	opts
}
,buf
{
	opts.bufsz?: 16_KiB
}
,event_id
{
	opts.event_id
}
,room_id
{
	opts.room_id
}
{
	this->opts.event_id = this->event_id;
	this->opts.room_id = this->room_id;
}
