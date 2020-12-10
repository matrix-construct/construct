// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::fed::well_known
{
	static net::hostport make_remote(const string_view &);
	static void submit(request &);
	static void receive(request &);
	static void finish(request &);
	static bool handle(request &);
	static void worker();

	static server::request request_skip;
	extern ctx::dock worker_dock;
	extern ctx::context worker_context;
	extern run::changed handle_quit;
	extern log::log log;
}

template<>
decltype(ircd::util::instance_list<ircd::m::fed::well_known::request>::allocator)
ircd::util::instance_list<ircd::m::fed::well_known::request>::allocator
{};

template<>
decltype(ircd::util::instance_list<ircd::m::fed::well_known::request>::list)
ircd::util::instance_list<ircd::m::fed::well_known::request>::list
{
    allocator
};

decltype(ircd::m::fed::well_known::log)
ircd::m::fed::well_known::log
{
	"m.well-known"
};

decltype(ircd::m::fed::well_known::cache_default)
ircd::m::fed::well_known::cache_default
{
	{ "name",     "ircd.m.fed.well-known.cache.default" },
	{ "default",  24 * 60 * 60L                         },
};

decltype(ircd::m::fed::well_known::cache_error)
ircd::m::fed::well_known::cache_error
{
	{ "name",     "ircd.m.fed.well-known.cache.error" },
	{ "default",  36 * 60 * 60L                       },
};

///NOTE: not yet used until HTTP cache headers in response are respected.
decltype(ircd::m::fed::well_known::cache_max)
ircd::m::fed::well_known::cache_max
{
	{ "name",     "ircd.m.fed.well-known.cache.max" },
	{ "default",  48 * 60 * 60L                     },
};

decltype(ircd::m::fed::well_known::request::path)
ircd::m::fed::well_known::request::path
{
	"/.well-known/matrix/server"
};

decltype(ircd::m::fed::well_known::request::type)
ircd::m::fed::well_known::request::type
{
	"well-known.matrix.server"
};

decltype(ircd::m::fed::well_known::request::sopts)
ircd::m::fed::well_known::request::sopts
{
	false // http_exceptions
};

decltype(ircd::m::fed::well_known::request::timeout)
ircd::m::fed::well_known::request::timeout
{
	{ "name",     "ircd.m.fed.well-known.request.timeout" },
	{ "default",  15L                                     },
};

decltype(ircd::m::fed::well_known::request::redirects_max)
ircd::m::fed::well_known::request::redirects_max
{
	{ "name",     "ircd.m.fed.well-known.request.redirects.max" },
	{ "default",  2L                                            },
};

decltype(ircd::m::fed::well_known::request::id_ctr)
ircd::m::fed::well_known::request::id_ctr;

decltype(ircd::m::fed::well_known::request::mutex)
ircd::m::fed::well_known::request::mutex;

decltype(ircd::m::fed::well_known::worker_dock)
ircd::m::fed::well_known::worker_dock;

decltype(ircd::m::fed::well_known::worker_context)
ircd::m::fed::well_known::worker_context
{
	"m.fed.well_known",
	512_KiB,
	&worker,
	context::POST
};

decltype(ircd::m::fed::well_known::handle_quit)
ircd::m::fed::well_known::handle_quit
{
	run::level::QUIT, []
	{
		worker_dock.notify_all();
	}
};

ircd::ctx::future<ircd::string_view>
ircd::m::fed::well_known::get(const mutable_buffer &buf,
                              const string_view &target,
                              const opts &opts)
try
{
	const m::room::id::buf cache_room_id
	{
		"dns", m::my_host()
	};

	const m::room cache_room
	{
		cache_room_id
	};

	const m::event::idx event_idx
	{
		likely(opts.cache_check)?
			cache_room.get(std::nothrow, request::type, target):
			0UL
	};

	const milliseconds origin_server_ts
	{
		m::get<time_t>(std::nothrow, event_idx, "origin_server_ts", time_t(0))
	};

	const json::object content
	{
		m::get(std::nothrow, event_idx, "content", buf)
	};

	const seconds ttl
	{
		content.get<time_t>("ttl", time_t(86400))
	};

	const system_point expires
	{
		origin_server_ts + ttl
	};

	const bool expired
	{
		ircd::now<system_point>() > expires
	};

	const json::string cached
	{
		content["m.server"]
	};

	const bool valid
	{
		// entry must not be blank
		!empty(cached)

		// entry must not be expired unless options allow expired hits
		&& (!expired || opts.expired)
	};

	// Branch to return cache hit
	if(likely(valid))
		return ctx::future<string_view>
		{
			ctx::already, string_view
			{
				data(buf), move(buf, cached)
			}
		};

	const net::hostport remote
	{
		make_remote(target)
	};

	const bool fetch
	{
		// options must allow network request
		opts.request

		// check if the peer already has a cached error in server::
		&& !server::errant(remote)
	};

	// Branch if won't fetch; return target itself as result
	if(!fetch)
		return ctx::future<string_view>
		{
			ctx::already, string_view
			{
				data(buf), move(buf, target)
			}
		};

	if(opts.cache_check)
	{
		char tmbuf[48];
		log::dwarning
		{
			log, "%s cache invalid %s event_idx:%u expires %s",
			target,
			cached?: json::string{"<not found>"},
			event_idx,
			cached? timef(tmbuf, expires, localtime): "<never>"_sv,
		};
	}

	// Synchronize modification of the request::list
	const std::lock_guard request_lock
	{
		request::mutex
	};

	// Start request
	auto req
	{
		std::make_unique<request>()
	};

	// req->target is a copy in the request struct so the caller can disappear.
	req->target = string_view
	{
		req->tgtbuf[0], copy(req->tgtbuf[0], target)
	};

	// req->m_server is a copy in the request struct so the caller can disappear.
	req->m_server = string_view
	{
		req->tgtbuf[1], copy(req->tgtbuf[1], cached)
	};

	// but req->out is not safe once the caller destroys their future, which
	// is indicated by req->promise's invalidation. Do not write to this
	// unless the promise is valid.
	req->out = buf;

	// all other properties are independent of the caller
	req->opts = opts;
	req->expires = expires;
	req->uri.path = request::path;
	req->uri.remote = req->target;
	ctx::future<string_view> ret{req->promise}; try
	{
		submit(*req);
		req.release();
	}
	catch(const std::exception &e)
	{
		log::derror
		{
			log, "request submit for %s :%s",
			target,
			e.what(),
		};

		const ctx::exception_handler eh;
		finish(*req);
	}

	return ret;
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "get %s :%s",
		target,
		e.what(),
	};

	return ctx::future<string_view>
	{
		ctx::already, string_view
		{
			data(buf), move(buf, target)
		}
	};
}

void
ircd::m::fed::well_known::worker()
try
{
	// Wait for runlevel RUN before proceeding...
	run::barrier<ctx::interrupted>{};
	while(!request::list.empty() || run::level == run::level::RUN)
	{
		worker_dock.wait([]
		{
			return !request::list.empty() || run::level != run::level::RUN;
		});

		if(request::list.empty())
			break;

		auto next
		{
			ctx::when_any(std::begin(request::list), std::end(request::list), []
			(auto &it) -> server::request &
			{
				return !(*it)->req? request_skip: (*it)->req;
			})
		};

		const ctx::uninterruptible::nothrow ui;
		if(!next.wait(milliseconds(250), std::nothrow))
			continue;

		const auto it
		{
			next.get()
		};

		assert(it != std::end(request::list));
		if(unlikely(it == std::end(request::list)))
			continue;

		const std::lock_guard request_lock
		{
			request::mutex
		};

		std::unique_ptr<request> req
		{
			*it
		};

		if(!handle(*req)) // redirect
			req.release();
		else
			finish(*req);
	}

	assert(request::list.empty());
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Worker unhandled :%s",
		e.what(),
	};
}

bool
ircd::m::fed::well_known::handle(request &req)
try
{
	receive(req);

	// Successful result
	if(likely(req.code < 300))
		return true;

	// Successful error; bail
	if(req.code >= 400)
		return true;

	// Indirection code, but no location response header
	if(!req.location)
		return true;

	// Redirection; carry over the new target by copying it because it's
	// in the buffer which we'll be overwriting for the new request.
	req.carry = unique_mutable_buffer{req.location};
	req.uri = string_view{req.carry};

	// Indirection code, bad location header.
	if(!req.uri.path || !req.uri.remote)
		return true;

	if(req.redirects++ >= request::redirects_max)
		return true;

	// Redirect
	submit(req);
	return false;
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "%s handling :%s",
		req.target,
		e.what(),
	};

	return true;
}

void
ircd::m::fed::well_known::finish(request &req)
try
{
	json::string result;
	const unwind resolve{[&req, &result]
	{
		if(req.promise.valid())
			req.promise.set_value(string_view
			{
				data(req.out), move(req.out, result?: req.target)
			});
	}};

	if(req.code == 200 && json::valid(req.response, std::nothrow))
		result = req.response["m.server"];

	if(!result)
		result = req.m_server;

	if(!result)
		result = req.target;

	if(result != req.target)
		log::debug
		{
			log, "query to %s for %s resolved delegation to %s",
			req.uri.remote,
			req.target,
			result,
		};

	// This construction validates we didn't get a junk string
	volatile const net::hostport ret
	{
		result
	};

	const bool cache_expired
	{
		req.expires < now<system_point>()
	};

	const bool cache_result
	{
		result
		&& req.opts.cache_result
		&& req.opts.request
		&& (cache_expired || result != req.m_server)
	};

	req.m_server = result;
	if(!cache_result)
		return;

	// Any time the well-known result is the same as the req.target (that
	// includes legitimate errors where fetch_well_known() returns the
	// req.target to default) we consider that an error and use the error
	// TTL value. Sorry, no exponential backoff implemented yet.
	const auto cache_ttl
	{
		req.target == req.m_server?
			seconds(cache_error).count():
			seconds(cache_default).count()
	};

	const m::room::id::buf cache_room_id
	{
		"dns", m::my_host()
	};

	// Write our record to the cache room; note that this doesn't really
	// match the format of other DNS records in this room since it's a bit
	// simpler, but we don't share the ircd.dns.rr type prefix anyway.
	const auto cache_id
	{
		m::send(cache_room_id, m::me(), request::type, req.target, json::members
		{
			{ "ttl",       cache_ttl    },
			{ "m.server",  req.m_server },
		})
	};

	log::debug
	{
		log, "%s cached delegation to %s with %s ttl:%ld",
		req.target,
		req.m_server,
		string_view{cache_id},
		cache_ttl,
	};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "%s completion :%s",
		req.target,
		e.what(),
	};
}

void
ircd::m::fed::well_known::receive(request &req)
{
	assert(req.sopts.http_exceptions == false);
	req.code = req.req.get(seconds(request::timeout));
	req.head = req.req.in.gethead(req.req);
	req.location = req.head.location;
	req.response = json::object
	{
		req.req.in.content
	};

	log::debug
	{
		log, "fetch to %s %s :%u %s",
		req.uri.remote,
		req.uri.path,
		uint(req.code),
		http::status(req.code),
	};
}

void
ircd::m::fed::well_known::submit(request &req)
{
	const net::hostport target
	{
		make_remote(req.uri.remote)
	};

	const http::header headers[]
	{
		{ "User-Agent", info::user_agent },
	};

	window_buffer wb
	{
		req.buf
	};

	http::request
	{
		wb, host(target), "GET", req.uri.path, 0, {}, headers
	};

	server::out out;
	out.head = wb.completed();

	// Remaining space in buffer is used for received head; note that below
	// we specify this same buffer for in.content, but that's a trick
	// recognized by ircd::server to place received content directly after
	// head in this buffer without any additional dynamic allocation.
	server::in in;
	in.head = mutable_buffer{req.buf} + size(out.head);
	in.content = in.head;

	req.code = http::code(0);
	req.head = {};
	req.response = {};
	req.location = {};
	req.req = server::request
	{
		target, std::move(out), std::move(in), &req.sopts
	};

	worker_dock.notify();
}

ircd::net::hostport
ircd::m::fed::well_known::make_remote(const string_view &target)
{
	const net::hostport remote
	{
		target
	};

	// Hard target https service; do not inherit any matrix service from remote.
	const net::hostport ret
	{
		net::host(remote), "https", net::port(remote)
	};

	return ret;
}
