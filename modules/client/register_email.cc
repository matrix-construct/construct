// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::register_email
{
	extern const string_view description;
	extern mods::import<ircd::conf::item<bool>> register_user_enable;
	extern mods::import<ircd::conf::item<bool>> register_enable;
}

namespace ircd::m::register_email::requesttoken
{
	static resource::response post(client &c, const resource::request &);

	extern resource::method method_post;
	extern resource _resource;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client 3.4.1 :Register Available"
};

decltype(ircd::m::register_email::description)
ircd::m::register_email::description
{R"(
	(5.5.2) The homeserver must check that the given email address is
	not already associated with an account on this homeserver. The
	homeserver has the choice of validating the email address itself,
	or proxying the request to the /validate/email/requestToken Identity
	Service API. The request should be proxied to the domain that is sent
	by the client in the id_server. It is imperative that the homeserver
	keep a list of trusted Identity Servers and only proxies to those it
	trusts.
)"};

decltype(ircd::m::register_email::requesttoken::_resource)
ircd::m::register_email::requesttoken::_resource
{
	"/_matrix/client/r0/register/email/requestToken",
	{
		description
	}
};

decltype(ircd::m::register_email::requesttoken::method_post)
ircd::m::register_email::requesttoken::method_post
{
	_resource, "POST", post
};

decltype(ircd::m::register_email::register_enable)
ircd::m::register_email::register_enable
{
	"client_register", "register_enable"
};

decltype(ircd::m::register_email::register_user_enable)
ircd::m::register_email::register_user_enable
{
	"client_register", "register_user_enable"
};

ircd::resource::response
ircd::m::register_email::requesttoken::post(client &client,
                                            const resource::request &request)
{
	// can't throw or Riot infinite loops hitting the server.
	if((0) && (!bool(register_enable) || !bool(register_user_enable)))
		throw m::error
		{
			http::OK, "M_REGISTRATION_DISABLED",
			"Registration is disabled. Nothing to email you."
		};

	const json::string &client_secret
	{
		request.at("client_secret")
	};

	const json::string &email
	{
		request.at("email")
	};

	const ushort send_attempt
	{
		request.at<ushort>("send_attempt")
	};

	const json::string &next_link
	{
		request["next_link"]
	};

	const json::string &id_server
	{
		request["id_server"]?
			request["id_server"]:
			my_host()
	};

	log::debug
	{
		m::log, "Verification email to [%s] attempt:%u idserv[%s]  next_link[%s]",
		email,
		send_attempt,
		id_server,
		next_link,
	};

	char sidbuf[256];
	const string_view sid
	{

	};

	return resource::response
	{
		client, json::members
		{
			{ "sid", sid }
		}
	};
}
