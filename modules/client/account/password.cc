// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "account.h"

using namespace ircd;

resource
account_password
{
	"/_matrix/client/r0/account/password",
	{
		"Changes the password for an account on this homeserver. (3.3.4)"
	}
};

resource::response
post__password(client &client,
               const resource::request &request)
try
{
	const json::string &new_password
	{
		request.at("new_password")
	};

	const json::object &auth
	{
		request["auth"]
	};

	const json::string &type
	{
		auth.at("type")
	};

	if(type != "m.login.password")
		throw m::error
		{
			"M_UNSUPPORTED", "Login type is not supported."
		};

	const json::string &session
	{
		auth["session"]
	};

	m::user user
	{
		request.user_id
	};

	user.password(new_password);
	return resource::response
	{
		client, http::OK
	};
}
catch(const db::not_found &e)
{
	throw m::error
	{
		http::FORBIDDEN, "M_FORBIDDEN", "Access denied."
	};
}

resource::method
post_password
{
	account_password, "POST", post__password,
	{
		post_password.REQUIRES_AUTH
	}
};

static string_view
gen_password_hash(const mutable_buffer &out,
                  const string_view &supplied_password);

extern "C" m::event::id::buf
set_password(const m::user::id &user_id,
             const string_view &password)
{
	char buf[64];
	const auto supplied
	{
		gen_password_hash(buf, password)
	};

	const m::user::room user_room{user_id};
	return send(user_room, user_id, "ircd.password", user_id,
	{
		{ "sha256", supplied }
	});
}

extern "C" bool
is_password(const m::user::id &user_id,
            const string_view &password)
noexcept try
{
	char buf[64];
	const auto supplied
	{
		gen_password_hash(buf, password)
	};

	bool ret{false};
	const m::user::room user_room{user_id};
	user_room.get("ircd.password", user_id, [&supplied, &ret]
	(const m::event &event)
	{
		const json::object &content
		{
			json::at<"content"_>(event)
		};

		const auto &correct
		{
			unquote(content.at("sha256"))
		};

		ret = supplied == correct;
	});

	return ret;
}
catch(const m::NOT_FOUND &e)
{
	return false;
}
catch(const std::exception &e)
{
	log::critical
	{
		"is_password__user(): %s %s",
		string_view{user_id},
		e.what()
	};

	return false;
}

string_view
gen_password_hash(const mutable_buffer &out,
                  const string_view &supplied_password)
{
	//TODO: ADD SALT
	const sha256::buf hash
	{
		sha256{supplied_password}
	};

	return b64encode_unpadded(out, hash);
}
