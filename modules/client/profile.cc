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

[[noreturn]] static void rethrow(const std::exception_ptr &, const m::user &, const string_view &);
static std::exception_ptr fetch_profile_remote(const m::user &, const string_view &);
static resource::response get__profile(client &, const resource::request &);
static resource::response put__profile(client &, const resource::request &);

mapi::header
IRCD_MODULE
{
	"Client 8.2 :Profiles"
};

const size_t
PARAM_MAX_SIZE
{
	128
};

ircd::resource
profile_resource
{
	"/_matrix/client/r0/profile/",
	{
		"(8.2) Profiles",
		resource::DIRECTORY,
	}
};

resource::method
method_get
{
	profile_resource, "GET", get__profile
};

resource::method
method_put
{
	profile_resource, "PUT", put__profile,
	{
		method_put.REQUIRES_AUTH
	}
};

resource::response
put__profile(client &client,
              const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"user_id path parameter required"
		};

	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"profile property path parameter required"
		};

	m::user::id::buf user_id
	{
		url::decode(user_id, request.parv[0])
	};

	if(user_id != request.user_id)
		throw m::FORBIDDEN
		{
			"Trying to set profile for '%s' but you are '%s'",
			user_id,
			request.user_id
		};

	const m::user user
	{
		user_id
	};

	char parambuf[PARAM_MAX_SIZE];
	const string_view &param
	{
		url::decode(parambuf, request.parv[1])
	};

	const string_view &value
	{
		request.at(param)
	};

	const m::user::profile profile
	{
		user
	};

	bool modified{true};
	profile.get(std::nothrow, param, [&value, &modified]
	(const string_view &param, const string_view &existing)
	{
		modified = existing != value;
	});

	if(!modified)
		return resource::response
		{
			client, http::OK
		};

	const auto eid
	{
		profile.set(param, value)
	};

	return resource::response
	{
		client, http::OK
	};
}

resource::response
get__profile(client &client,
             const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"user_id path parameter required"
		};

	m::user::id::buf user_id
	{
		url::decode(user_id, request.parv[0])
	};

	const m::user user
	{
		user_id
	};

	char parambuf[PARAM_MAX_SIZE];
	const string_view &param
	{
		request.parv.size() > 1?
			url::decode(parambuf, request.parv[1]):
			string_view{}
	};

	// For remote users, we try to get the latest profile data
	// from the remote server and cache it locally. When there's
	// a problem, we store that problem in this eptr for later.
	const std::exception_ptr eptr
	{
		!my(user)?
			fetch_profile_remote(user, param):
			std::exception_ptr{}
	};

	// Now we treat the profile as local data in any case
	const m::user::profile profile
	{
		user
	};

	if(param) try
	{
		// throws if param not found
		profile.get(param, [&client]
		(const string_view &param, const string_view &value)
		{
			resource::response
			{
				client, json::members
				{
					{ param, value }
				}
			};
		});

		return {};
	}
	catch(const std::exception &e)
	{
		// If there was a problem querying locally for this param and the
		// user is remote, eptr will have a better error for the client.
		if(!my(user))
			rethrow(eptr, user, param);

		throw;
	}

	// Have to return a 404 if the profile is empty rather than a {},
	// so we iterate for at least one element first to check that.
	bool empty{true};
	profile.for_each([&empty]
	(const string_view &, const string_view &)
	{
		empty = false;
		return false;
	});

	// If we have no profile data and the user is not ours, eptr might have
	// a better error for our client.
	if(empty && !my(user))
		rethrow(eptr, user, param);

	// Otherwise if there's no profile data we 404 our client.
	if(empty)
		throw m::NOT_FOUND
		{
			"Profile for %s is empty.",
			string_view{user.user_id}
		};

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::object top
	{
		out
	};

	profile.for_each([&top]
	(const string_view &param, const string_view &value)
	{
		json::stack::member
		{
			top, param, value
		};

		return true;
	});

	return {};
}

std::exception_ptr
fetch_profile_remote(const m::user &user,
                     const string_view &key)
try
{
	m::user::profile::fetch(user, user.user_id.host(), key);
	return {};
}
catch(const std::exception &e)
{
	return std::current_exception();
}

void
rethrow(const std::exception_ptr &eptr,
        const m::user &user,
        const string_view &key)
try
{
	std::rethrow_exception(eptr);
}
catch(const http::error &)
{
	throw;
}
catch(const ctx::timeout &)
{
	throw m::error
	{
		http::GATEWAY_TIMEOUT, "M_PROFILE_TIMEOUT",
		"Server '%s' did not respond with profile for %s in time.",
		user.user_id.host(),
		string_view{user.user_id}
	};
}
catch(const server::unavailable &e)
{
	throw m::error
	{
		http::SERVICE_UNAVAILABLE, "M_PROFILE_UNAVAILABLE",
		"Server '%s' cannot be contacted for profile of %s :%s",
		user.user_id.host(),
		string_view{user.user_id},
		e.what()
	};
}
catch(const server::error &e)
{
	throw m::error
	{
		http::BAD_GATEWAY, "M_PROFILE_UNAVAILABLE",
		"Error when contacting '%s' for profile of %s :%s",
		user.user_id.host(),
		string_view{user.user_id},
		e.what()
	};
}
