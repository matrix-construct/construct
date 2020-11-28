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
	static void _check_event(const request &, const m::event &);
	static void check_response(const request &, const json::object &);
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
	{ "default",  256L                        },
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
		it = requests.emplace_hint(it, opts);

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
		if(!!request.finished)
			continue;

		else if(!request.started)
			start(request);

		else if(!request.finished && timedout(request, now))
			retry(request);
	}

	auto it(begin(requests)); while(it != end(requests))
	{
		auto &request(mutable_cast(*it));
		if(!!request.finished)
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
	assert(!request.finished);
	if(unlikely(run::level != run::level::RUN))
		throw m::UNAVAILABLE
		{
			"Cannot start fetch requests in runlevel."
		};

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

	#if 0
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
	strlcpy(res.origin, request.origin);
	request.promise.set_value(std::move(res));
}

namespace ircd::m::fetch
{
	static void check_response__backfill(const request &, const json::object &);
	static void check_response__event(const request &, const json::object &);
	static void check_response__auth(const request &, const json::object &);

	extern conf::item<bool> check_event_id;
	extern conf::item<bool> check_conforms;
	extern conf::item<bool> check_signature;
	extern conf::item<bool> check_hashes;
	extern conf::item<bool> check_authoritative_redaction;
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
	{ "default",  true                          },
};

decltype(ircd::m::fetch::check_hashes)
ircd::m::fetch::check_hashes
{
	{ "name",     "ircd.m.fetch.check.hashes" },
	{ "default",  true                        },
};

decltype(ircd::m::fetch::check_authoritative_redaction)
ircd::m::fetch::check_authoritative_redaction
{
	{ "name",     "ircd.m.fetch.check.authoritative_redaction" },
	{ "default",  true                                         },
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

	for(const json::object auth_event : auth_chain)
	{
		m::event::id::buf event_id;
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

	for(const json::object event : pdus)
	{
		m::event::id::buf event_id;
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
	if(request.opts.check_event_id && check_event_id && !m::check_id(event))
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

	if(request.opts.check_conforms && check_conforms)
	{
		m::event::conforms conforms
		{
			event
		};

		const bool mismatch_hashes
		{
			check_hashes
			&& request.opts.check_hashes
			&& conforms.has(m::event::conforms::MISMATCH_HASHES)
		};

		const bool authoritative_redaction
		{
			check_authoritative_redaction
			&& request.opts.authoritative_redaction
			&& mismatch_hashes
			&& json::get<"origin"_>(event) == request.origin
		};

		if(authoritative_redaction || !mismatch_hashes)
			conforms.del(m::event::conforms::MISMATCH_HASHES);

		thread_local char buf[128];
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

	// only check signature for v1 events
	if(request.opts.check_signature && check_signature && request.opts.event_id.version() == "1")
	{
		const string_view &server
		{
			!json::get<"origin"_>(event)?
				user::id(at<"sender"_>(event)).host():
				string_view(json::get<"origin"_>(event))
		};

		const json::object &signatures
		{
			at<"signatures"_>(event).at(server)
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

ircd::m::fetch::request::~request()
noexcept
{
	//TODO: bad things unless this first here
	future.reset(nullptr);
}
