// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	extern conf::item<seconds> cache_warmup_time;
	extern conf::item<bool> x_matrix_verify_origin;
	extern conf::item<bool> x_matrix_verify_destination;

	static user::id::buf authenticate_user(const resource::method &, const client &, resource::request &);
	static string_view authenticate_node(const resource::method &, const client &, resource::request &);
	static void cache_warm_origin(const resource::request &);
}

decltype(ircd::m::cache_warmup_time)
ircd::m::cache_warmup_time
{
	{ "name",     "ircd.m.cache_warmup_time" },
	{ "default",  3600L                      },
};

decltype(ircd::m::x_matrix_verify_origin)
ircd::m::x_matrix_verify_origin
{
	{ "name",     "ircd.m.x_matrix.verify_origin" },
	{ "default",  true                            },
};

decltype(ircd::m::x_matrix_verify_origin)
ircd::m::x_matrix_verify_destination
{
	{ "name",     "ircd.m.x_matrix.verify_destination" },
	{ "default",  true                                 },
};

//
// m::resource::method
//

ircd::m::resource::method::method(m::resource &resource,
                                  const string_view &name,
                                  handler function,
                                  struct opts opts)
:ircd::resource::method
{
	resource,
	name,
	std::bind(&method::handle, this, ph::_1, ph::_2),
	std::move(opts),
}
,function
{
	std::move(function)
}
{
}

ircd::m::resource::method::~method()
noexcept
{
}

ircd::resource::response
ircd::m::resource::method::handle(client &client,
                                  ircd::resource::request &request_)
try
{
	m::resource::request request
	{
		*this, client, request_
	};

	if(request.origin)
	{
		// If we have an error cached from previously not being able to
		// contact this origin we can clear that now that they're alive.
		server::errclear(fed::matrix_service(request.origin));

		// The origin was verified so we can invoke the cache warming now.
		cache_warm_origin(request);
	}

	return function(client, request);
}
catch(const json::print_error &e)
{
	throw m::error
	{
		http::INTERNAL_SERVER_ERROR, "M_NOT_JSON",
		"Generator Protection: %s",
		e.what()
	};
}
catch(const json::not_found &e)
{
	throw m::error
	{
		http::BAD_REQUEST, "M_BAD_JSON",
		"Required JSON field: %s",
		e.what()
	};
}
catch(const json::error &e)
{
	throw m::error
	{
		http::BAD_REQUEST, "M_NOT_JSON",
		"%s",
		e.what()
	};
}
catch(const ctx::timeout &e)
{
	throw m::error
	{
		http::BAD_GATEWAY, "M_REQUEST_TIMEOUT",
		"%s",
		e.what()
	};
}

//
// resource::request
//

ircd::m::resource::request::request(const method &method,
                                    const client &client,
                                    ircd::resource::request &r)
:ircd::resource::request
{
	r
}
,authorization
{
	split(head.authorization, ' ')
}
,access_token
{
	iequals(authorization.first, "Bearer"_sv)?
		authorization.second:
		query["access_token"]
}
,x_matrix
{
	!access_token && iequals(authorization.first, "X-Matrix"_sv)?
		m::request::x_matrix{authorization.first, authorization.second}:
		m::request::x_matrix{}
}
,node_id
{
	//NOTE: may be assigned by authenticate_node()
}
,origin
{
	// Server X-Matrix header verified here. Similar to client auth, origin
	// which has been authed is referenced in the client.request. If the method
	// requires, and auth fails or not provided, this function throws.
	// Otherwise it returns a string_view of the origin name in
	// client.request.origin, or an empty string_view if an origin was not
	// apropos for this request (i.e a client request rather than federation).
	authenticate_node(method, client, *this)
}
,user_id
{
	// Client access token verified here. On success, user_id owning the token
	// is copied into the client.request structure. On failure, the method is
	// checked to see if it requires authentication and if so, this throws.
	authenticate_user(method, client, *this)
}
{
}

/// Authenticate a client based on access_token either in the query string or
/// in the Authentication bearer header. If a token is found the user_id owning
/// the token is copied into the request. If it is not found or it is invalid
/// then the method being requested is checked to see if it is required. If so
/// the appropriate exception is thrown.
ircd::m::user::id::buf
ircd::m::authenticate_user(const resource::method &method,
                           const client &client,
                           resource::request &request)
{
	assert(method.opts);
	const auto requires_auth
	{
		method.opts->flags & resource::method::REQUIRES_AUTH
	};

	m::user::id::buf ret;
	if(!request.access_token && !requires_auth)
		return ret;

	if(!request.access_token)
		throw m::error
		{
			http::UNAUTHORIZED, "M_MISSING_TOKEN",
			"Credentials for this method are required but missing."
		};

	static const m::event::fetch::opts fopts
	{
		m::event::keys::include {"sender"}
	};

	const m::room::id::buf tokens_room
	{
		"tokens", origin(my())
	};

	const m::room::state tokens
	{
		tokens_room, &fopts
	};

	tokens.get(std::nothrow, "ircd.access_token", request.access_token, [&ret]
	(const m::event &event)
	{
		// The user sent this access token to the tokens room
		ret = at<"sender"_>(event);
	});

	if(requires_auth && !ret)
		throw m::error
		{
			http::UNAUTHORIZED, "M_UNKNOWN_TOKEN",
			"Credentials for this method are required but invalid."
		};

	return ret;
}

ircd::string_view
ircd::m::authenticate_node(const resource::method &method,
                           const client &client,
                           resource::request &request)
try
{
	assert(method.opts);
	const auto required
	{
		method.opts->flags & resource::method::VERIFY_ORIGIN
	};

	const bool supplied
	{
		!empty(x_matrix.origin)
	};

	if(!required && !supplied)
		return {};

	if(required && !supplied)
		throw m::error
		{
			http::UNAUTHORIZED, "M_MISSING_AUTHORIZATION",
			"Required X-Matrix Authorization was not supplied"
		};

	if(x_matrix_verify_destination && !m::my_host(request.head.host))
		throw m::error
		{
			http::UNAUTHORIZED, "M_NOT_MY_HOST",
			"The X-Matrix Authorization destination '%s' is not recognized here.",
			request.head.host
		};

	const m::request object
	{
		x_matrix.origin,
		request.head.host,
		method.name,
		request.head.uri,
		request.content
	};

	if(x_matrix_verify_origin && !object.verify(x_matrix.key, x_matrix.sig))
		throw m::error
		{
			http::FORBIDDEN, "M_INVALID_SIGNATURE",
			"The X-Matrix Authorization is invalid."
		};

	request.node_id = request.origin; //TODO: remove request.node_id.
	return x_matrix.origin;
}
catch(const m::error &)
{
	throw;
}
catch(const std::exception &e)
{
	log::derror
	{
		resource::log, "X-Matrix Authorization from %s: %s",
		string(remote(client)),
		e.what()
	};

	throw m::error
	{
		http::UNAUTHORIZED, "M_UNKNOWN_ERROR",
		"An error has prevented authorization: %s",
		e.what()
	};
}

/// We can smoothly warmup some memory caches after daemon startup as the
/// requests trickle in from remote servers. This function is invoked after
/// a remote contacts and reveals its identity with the X-Matrix verification.
///
/// This process helps us avoid cold caches for the first requests coming from
/// our server. Such requests are often parallel requests, for ex. to hundreds
/// of servers in a Matrix room at the same time.
///
/// This function does nothing after the cache warmup period has ended.
void
ircd::m::cache_warm_origin(const resource::request &request)
try
{
	assert(request.origin);
	if(ircd::uptime() > seconds(cache_warmup_time))
		return;

	// Make a query through SRV and A records.
	//net::dns::resolve(origin, net::dns::prefetch_ipport);
}
catch(const std::exception &e)
{
	log::derror
	{
		resource::log, "Cache warming for '%s' :%s",
		request.origin,
		e.what()
	};
}
