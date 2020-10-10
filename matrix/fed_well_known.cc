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
	struct request;

	static void submit(request &);
	static void receive(request &);
	static int handle(request &);

	extern log::log log;
}

struct ircd::m::fed::well_known::request
{
	static const string_view path;
	static const server::request::opts sopts;

	unique_mutable_buffer buf;
	unique_mutable_buffer carry;
	rfc3986::uri uri;
	server::request req;
	http::code code {0};
	http::response::head head;
	string_view location;
	size_t redirects {0};
	json::object response;
	json::string m_server;
};

decltype(ircd::m::fed::well_known::log)
ircd::m::fed::well_known::log
{
	"m.well-known"
};

decltype(ircd::m::fed::well_known::request::path)
ircd::m::fed::well_known::request::path
{
	"/.well-known/matrix/server"
};

decltype(ircd::m::fed::well_known::request::sopts)
ircd::m::fed::well_known::request::sopts
{
	false // http_exceptions
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

decltype(ircd::m::fed::well_known::fetch_timeout)
ircd::m::fed::well_known::fetch_timeout
{
	{ "name",     "ircd.m.fed.well-known.fetch.timeout" },
	{ "default",  15L                                   },
};

decltype(ircd::m::fed::well_known::fetch_redirects)
ircd::m::fed::well_known::fetch_redirects
{
	{ "name",     "ircd.m.fed.well-known.fetch.redirects" },
	{ "default",  2L                                      },
};

ircd::string_view
ircd::m::fed::well_known::get(const mutable_buffer &buf,
                              const string_view &origin)
try
{
	static const string_view type
	{
		"well-known.matrix.server"
	};

	const m::room::id::buf room_id
	{
		"dns", m::my_host()
	};

	const m::room room
	{
		room_id
	};

	const m::event::idx event_idx
	{
		room.get(std::nothrow, type, origin)
	};

	const milliseconds origin_server_ts
	{
		m::get<time_t>(std::nothrow, event_idx, "origin_server_ts", time_t(0))
	};

	const json::object content
	{
		origin_server_ts > 0ms?
			m::get(std::nothrow, event_idx, "content", buf):
			const_buffer{}
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

	const string_view cached
	{
		data(buf), move(buf, json::string(content["m.server"]))
	};

	const bool valid
	{
		!empty(cached)
	};

	// Branch on valid cache hit to return result.
	if(valid && !expired)
	{
		char tmbuf[48];
		log::debug
		{
			log, "%s found in cache delegated to %s event_idx:%u expires %s",
			origin,
			cached,
			event_idx,
			timef(tmbuf, expires, localtime),
		};

		return cached;
	}

	// Crucial value that will provide us with a return string for this
	// function in any case. This is obtained by either using the value
	// found in cache or making a network query for a new value. expired=true
	// when a network query needs to be made, otherwise we can return the
	// cached value. If the network query fails, this value is still defaulted
	// as the origin string to return and we'll also cache that too.
	const string_view delegated
	{
		fetch(buf + size(cached), origin)
	};

	// Conditions for valid expired cache hit w/ failure to reacquire.
	const bool fallback
	{
		valid
		&& expired
		&& cached != delegated
		&& origin == delegated
		&& now<system_point>() < expires + seconds(cache_max)
	};

	if(fallback)
	{
		char tmbuf[48];
		log::debug
		{
			log, "%s found in cache delegated to %s event_idx:%u expired %s",
			origin,
			cached,
			event_idx,
			timef(tmbuf, expires, localtime),
		};

		return cached;
	}

	// Any time the well-known result is the same as the origin (that
	// includes legitimate errors where fetch_well_known() returns the
	// origin to default) we consider that an error and use the error
	// TTL value. Sorry, no exponential backoff implemented yet.
	const auto cache_ttl
	{
		origin == delegated?
			seconds(cache_error).count():
			seconds(cache_default).count()
	};

	// Write our record to the cache room; note that this doesn't really
	// match the format of other DNS records in this room since it's a bit
	// simpler, but we don't share the ircd.dns.rr type prefix anyway.
	const auto cache_id
	{
		m::send(room, m::me(), type, origin, json::members
		{
			{ "ttl",       cache_ttl  },
			{ "m.server",  delegated  },
		})
	};

	log::debug
	{
		log, "%s caching delegation to %s to cache in %s",
		origin,
		delegated,
		string_view{cache_id},
	};

	return delegated;
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "%s :%s",
		origin,
		e.what(),
	};

	return string_view
	{
		data(buf), move(buf, origin)
	};
}

ircd::string_view
ircd::m::fed::well_known::fetch(const mutable_buffer &user_buf,
                                const string_view &target)
try
{
	request req;
	req.uri.path = request::path;
	req.uri.remote = target;
	req.buf = unique_mutable_buffer
	{
		8_KiB
	};

	for(; req.redirects < fetch_redirects; ++req.redirects)
	{
		submit(req);
		receive(req);
		switch(handle(req))
		{
			case -1:
				continue;

			case false:
				break;

			case true:
				return string_view
				{
					data(user_buf), move(user_buf, req.m_server)
				};
		}
	}

	return string_view
	{
		data(user_buf), move(user_buf, target)
	};
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "%s in network query :%s",
		target,
		e.what(),
	};

	return string_view
	{
		data(user_buf), move(user_buf, target)
	};
}

int
ircd::m::fed::well_known::handle(request &req)
{
	// Successful result
	if(likely(req.code < 300))
	{
		req.m_server = req.response["m.server"];

		// This construction validates we didn't get a junk string
		volatile const net::hostport ret
		{
			req.m_server
		};

		log::debug
		{
			log, "query to %s found delegation to %s",
			req.uri.remote,
			req.m_server,
		};

		assert(bool(req.m_server));
		return true;
	}

	// Successful error; bail
	if(req.code >= 400)
		return false;

	// Indirection code, but no location response header
	if(!req.location)
		return false;

	// Redirection; carry over the new target by copying it because it's
	// in the buffer which we'll be overwriting for the new request.
	req.carry = unique_mutable_buffer{req.location};
	req.uri = string_view{req.carry};

	// Indirection code, bad location header.
	if(!req.uri.path || !req.uri.remote)
		return false;

	// Redirect
	return -1;
}

void
ircd::m::fed::well_known::receive(request &req)
{
	const seconds timeout
	{
		fetch_timeout
	};

	req.code = req.req.get(timeout);
	req.head = req.req.in.gethead(req.req);
	req.location = req.head.location;
	req.response = json::object
	{
		req.req.in.content
	};

	char dom_buf[rfc3986::DOMAIN_BUFSIZE];
	log::debug
	{
		log, "fetch from %s %s :%u %s",
		req.uri.remote,
		req.uri.path,
		uint(req.code),
		http::status(req.code),
	};
}

/// Launch requestReturn a tuple of the HTTP code, any Location header, and the response
/// content. These values are unconditional if this function doesn't throw,
/// but if there's no Location header and/or content then those will be empty
/// string_view's. This function is intended to be run in a loop by the caller
/// to chase redirection. No HTTP codes will throw from here; server and
/// network errors (and others) will.
void
ircd::m::fed::well_known::submit(request &req)
{
	const net::hostport &remote
	{
		req.uri.remote
	};

	// Hard target https service; do not inherit any matrix service from remote.
	const net::hostport target
	{
		net::host(remote), "https", net::port(remote)
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
	in.head = req.buf + size(out.head);
	in.content = in.head;

	req.req = server::request
	{
		target, std::move(out), std::move(in), &req.sopts
	};
}
