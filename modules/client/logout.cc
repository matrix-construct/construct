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

resource::response
post__logout(client &client, const resource::request &request)
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

	const auto token_event_id
	{
		m::event_id(token_event_idx)
	};

	static const string_view reason
	{
		"logout"
	};

	const auto redaction_event_id
	{
		m::redact(m::user::tokens, request.user_id, token_event_id, reason)
	};

	return resource::response
	{
		client, http::OK
	};
}

resource::method
post_method
{
	logout_resource, "POST", post__logout,
	{
		post_method.REQUIRES_AUTH
	}
};
