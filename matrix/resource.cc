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
	extern conf::item<bool> x_matrix_verify_origin;
	extern conf::item<bool> x_matrix_verify_destination;

	static string_view authenticate_bridge(const resource::method &, const client &, resource::request &);
	static user::id authenticate_user(const resource::method &, const client &, resource::request &);
	static string_view authenticate_node(const resource::method &, const client &, resource::request &);
}

decltype(ircd::m::resource::log)
ircd::m::resource::log
{
	"m.resource"
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
// m::resource
//

ircd::m::resource::resource(const string_view &path)
:m::resource
{
	path, {},
}
{
}

ircd::m::resource::resource(const string_view &path,
                            struct opts opts)
:ircd::resource
{
	path_canonize(path_buf, path), opts
}
{
}

ircd::m::resource::~resource()
noexcept
{
}

ircd::string_view
ircd::m::resource::params(const string_view &path)
const
{
	const auto prefix_tokens
	{
		token_count(this->path, '/')
	};

	const auto version
	{
		path_version(path)
	};

	const auto params_after
	{
		prefix_tokens? prefix_tokens - 1 + bool(version): 0
	};

	const auto ret
	{
		tokens_after(path, '/', params_after)
	};

	return ret;
}

ircd::resource &
ircd::m::resource::route(const string_view &path)
const
{
	thread_local char canon_buf[1024];
	const string_view canon_path
	{
		path_canonize(canon_buf, path)
	};

	return mutable_cast(ircd::resource::route(canon_path));
}

ircd::string_view
ircd::m::resource::path_canonize(const mutable_buffer &buf,
                                 const string_view &path)
{
	const auto version
	{
		path_version(path)
	};

	if(!version)
		return path;

	const auto &[before, after]
	{
		tokens_split(path, '/', 2, 1)
	};

	mutable_buffer out{buf};
	consume(out, copy(out, '/'));
	consume(out, copy(out, before));
	if(likely(after))
	{
		consume(out, copy(out, '/'));
		consume(out, copy(out, after));
	}

	return string_view
	{
		data(buf), data(out)
	};
}

ircd::string_view
ircd::m::resource::path_version(const string_view &path)
{
	const auto version
	{
		token(path, '/', 2, {})
	};

	const bool is_version
	{
		true
		&& version.size() >= 2
		&& (version[0] == 'v' || version[0] == 'r')
		&& (version[1] >= '0' && version[1] <= '9')
	};

	const bool pass
	{
		false
		|| is_version
		|| version == "unstable"
	};

	return pass? version: string_view{};
}

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

	const string_view &ident
	{
		request.bridge_id?:
		request.node_id?:
		request.user_id?:
		string_view{}
	};

	if(ident)
		log::debug
		{
			log, "%s %s %s %s `%s'",
			client.loghead(),
			ident,
			request.head.method,
			request.version?: "??"_sv,
			request.head.path,
		};

	const bool cached_error
	{
		request.node_id
		&& fed::errant(request.node_id)
	};

	// If we have an error cached from previously not being able to
	// contact this origin we can clear that now that they're alive.
	if(cached_error)
	{
		m::burst::opts opts;
		m::burst::burst
		{
			request.node_id, opts
		};
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
,version
{
	path_version(head.path)
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
	// Server X-Matrix header verified here. Similar to client auth, origin
	// which has been authed is referenced in the client.request. If the method
	// requires, and auth fails or not provided, this function throws.
	// Otherwise it returns a string_view of the origin name in
	// request.node_id, or an empty string_view if an origin was not
	// apropos for this request (i.e a client request rather than federation).
	authenticate_node(method, client, *this)
}
,user_id
{
	// Client access token verified here. On success, user_id owning the token
	// is copied into the request structure. On failure, the method is
	// checked to see if it requires authentication and if so, this throws.
	authenticate_user(method, client, *this)
}
,bridge_id
{
	// Application service access token verified here. Note that on success
	// this function will set the user_id as well as the bridge_id.
	authenticate_bridge(method, client, *this)
}
{
}

/// Authenticate a client based on access_token either in the query string or
/// in the Authentication bearer header. If a token is found the user_id owning
/// the token is copied into the request. If it is not found or it is invalid
/// then the method being requested is checked to see if it is required. If so
/// the appropriate exception is thrown. Note that if the access_token belongs
/// to a bridge (application service), the bridge_id is set accordingly.
ircd::m::user::id
ircd::m::authenticate_user(const resource::method &method,
                           const client &client,
                           resource::request &request)
{
	assert(method.opts);
	const auto requires_auth
	{
		method.opts->flags & resource::method::REQUIRES_AUTH
	};

	if(!requires_auth && !request.access_token)
		return {};

	// Note that we still try to auth a token and obtain a user_id here even
	// if the endpoint does not require auth; an auth'ed user may enjoy
	// additional functionality if credentials provided.
	if(requires_auth && !request.access_token)
		throw m::error
		{
			http::UNAUTHORIZED, "M_MISSING_TOKEN",
			"Credentials for this method are required but missing."
		};

	// Belay authentication to authenticate_bridge().
	if(startswith(request.access_token, "bridge_"))
		return {};

	const m::room::id::buf tokens_room_id
	{
		"tokens", origin(my())
	};

	const m::room::state tokens
	{
		tokens_room_id
	};

	const event::idx event_idx
	{
		tokens.get(std::nothrow, "ircd.access_token", request.access_token)
	};

	// The sender of the token is the user being authenticated.
	const string_view sender
	{
		m::get(std::nothrow, event_idx, "sender", request.id_buf)
	};

	// Note that if the endpoint does not require auth and we were not
	// successful in authenticating the provided token: we do not throw here;
	// instead we continue as if no token was provided, and no user_id will
	// be known to the requested endpoint.
	if(requires_auth && !sender)
		throw m::error
		{
			http::UNAUTHORIZED, "M_UNKNOWN_TOKEN",
			"Credentials for this method are required but invalid."
		};

	return sender;
}

/// Authenticate an application service (bridge)
ircd::string_view
ircd::m::authenticate_bridge(const resource::method &method,
                             const client &client,
                             resource::request &request)
{
	// Real user was already authenticated; not a bridge.
	if(request.user_id)
		return {};

	// No attempt at authenticating as a bridge; not a bridge.
	if(!startswith(request.access_token, "bridge_"))
		return {};

	const m::room::id::buf tokens_room_id
	{
		"tokens", origin(my())
	};

	const m::room::state tokens
	{
		tokens_room_id
	};

	const event::idx event_idx
	{
		tokens.get(std::nothrow, "ircd.access_token", request.access_token)
	};

	// The sender of the token is the bridge's user_id, where the bridge_id
	// is the localpart, but none of this is a puppetting/target user_id.
	const string_view sender
	{
		m::get(std::nothrow, event_idx, "sender", request.id_buf)
	};

	// Note that unlike authenticate_user, if an as_token was proffered but is
	// not valid, there is no possible fallback to unauthenticated mode and
	// this must throw here.
	if(!sender)
		throw m::error
		{
			http::UNAUTHORIZED, "M_UNKNOWN_TOKEN",
			"Credentials for this method are required but invalid."
		};

	// Find the user_id the bridge wants to masquerade as.
	const string_view puppet_user_id
	{
		request.query["user_id"]
	};

	// Set the user credentials for the request at the discretion of the
	// bridge. If the bridge did not supply a user_id then we set the value
	// to the bridge's own agency. Note that we urldecode into the id_buf
	// after the bridge_user_id; care not to overwrite it.
	{
		mutable_buffer buf(request.id_buf);
		request.user_id = puppet_user_id?
			url::decode(buf + size(sender), puppet_user_id):
			sender;
	}

	// Return only the localname (that's the localpart not including sigil).
	return m::user::id(sender).localname();
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
		!empty(request.x_matrix.origin)
	};

	if(!required && !supplied)
		return {};

	if(required && !supplied)
		throw m::error
		{
			http::UNAUTHORIZED, "M_MISSING_AUTHORIZATION",
			"Required X-Matrix Authorization was not supplied"
		};

	if(x_matrix_verify_destination && !m::self::host(request.head.host))
		throw m::error
		{
			http::UNAUTHORIZED, "M_NOT_MY_HOST",
			"The HTTP Host '%s' is not an authenticable destination here.",
			request.head.host,
		};

	const auto head_host
	{
		rstrip(request.head.host, ":8448")
	};

	const auto auth_dest
	{
		rstrip(request.x_matrix.destination, ":8448")
	};

	if(x_matrix_verify_destination && auth_dest && head_host != auth_dest)
		throw m::error
		{
			http::UNAUTHORIZED, "M_NOT_MY_DESTINATION",
			"The X-Matrix Authorization destination '%s' is not recognized here.",
			auth_dest,
		};

	const m::request object
	{
		request.x_matrix.origin,
		head_host,
		method.name,
		request.head.uri,
		request.content
	};

	if(x_matrix_verify_origin && !object.verify(request.x_matrix.key, request.x_matrix.sig))
		throw m::error
		{
			http::FORBIDDEN, "M_INVALID_SIGNATURE",
			"The X-Matrix Authorization is invalid."
		};

	return request.x_matrix.origin;
}
catch(const m::error &)
{
	throw;
}
catch(const std::exception &e)
{
	thread_local char rembuf[128];
	log::derror
	{
		resource::log, "X-Matrix Authorization from %s: %s",
		string(rembuf, remote(client)),
		e.what()
	};

	throw m::error
	{
		http::UNAUTHORIZED, "M_UNKNOWN_ERROR",
		"An error has prevented authorization: %s",
		e.what()
	};
}
