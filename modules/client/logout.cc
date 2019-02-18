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

static bool
do_logout(const m::user::id &user_id,
          const m::event::idx &token_event_idx);

static resource::response
post__logout(client &,
             const resource::request &);

static resource::response
post__logout_all(client &,
                 const resource::request &);

mapi::header
IRCD_MODULE
{
	"Client 3.3.2 :Logout"
};

resource
logout_resource
{
	"/_matrix/client/r0/logout",
	{
		"(3.3.2) Invalidates an existing access token, so that it can no"
		" longer be used for authorization."
	}
};

resource
logout_all_resource
{
	"/_matrix/client/r0/logout/all",
	{
		"(5.4.4) Invalidates all access tokens for a user, so that they can no"
		" longer be used for authorization. This includes the access token"
		" that made this request."
	}
};

resource::method
post_method
{
	logout_resource, "POST", post__logout,
	{
		post_method.REQUIRES_AUTH
	}
};

resource::method
post_all_method
{
	logout_all_resource, "POST", post__logout_all,
	{
		post_all_method.REQUIRES_AUTH
	}
};

resource::response
post__logout(client &client,
             const resource::request &request)
{
	const auto &access_token
	{
		request.access_token
	};

	const m::room::state tokens
	{
		m::user::tokens
	};

	const auto token_event_idx
	{
		tokens.get("ircd.access_token", access_token)
	};

	do_logout(request.user_id, token_event_idx);

	return resource::response
	{
		client, http::OK
	};
}

resource::response
post__logout_all(client &client,
                 const resource::request &request)
{
	const m::room::state tokens
	{
		m::user::tokens
	};

	long count(0);
	tokens.for_each("ircd.access_token", m::event::closure_idx{[&request, &count]
	(const m::event::idx &event_idx)
	{
		bool match(false);
		m::get(std::nothrow, event_idx, "sender", [&request, &match]
		(const string_view &sender)
		{
			match = request.user_id == sender;
		});

		if(match)
		{
			do_logout(request.user_id, event_idx);
			++count;
		}
	}});

	return resource::response
	{
		client, json::members
		{
			{ "invalidations", count }
		}
	};
}

bool
do_logout(const m::user::id &user_id,
          const m::event::idx &token_event_idx)
try
{
	static const string_view reason
	{
		"logout"
	};

	const auto token_event_id
	{
		m::event_id(token_event_idx)
	};

	const auto redaction_event_id
	{
		m::redact(m::user::tokens, user_id, token_event_id, reason)
	};

	return true;
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "Error logging out user '%s' token event_idx:%lu :%s",
		string_view{user_id},
		token_event_idx,
		e.what()
	};

	return false;
}
