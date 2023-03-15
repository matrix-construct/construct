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
	"Federation 2.2.1.1 :Publishing Keys"
};

m::resource
server_resource
{
	"/_matrix/key/v2/server/",
	{
		"federation 2.2.1.1: Publishing Keys",
		resource::DIRECTORY,
	}
};

conf::item<std::string>
occlusion_blacklist
{
	{ "name",     "ircd.key.occlude.blacklist" },
	{ "default",  ""                           },
};

conf::item<std::string>
occlusion_whitelist
{
	{ "name",     "ircd.key.occlude.whitelist" },
	{ "default",  ""                           },
};

m::resource::response
handle_get(client &client,
           const m::resource::request &request)
{
	char key_id_buf[256];
	const auto key_id
	{
		url::decode(key_id_buf, request.params)
	};

	const blackwhite::list acl
	{
		' ', occlusion_blacklist, occlusion_whitelist
	};

	char remote_buf[256];
	const auto ip_str
	{
		occlusion_blacklist || occlusion_whitelist?
			host(string(remote_buf, remote(client))):
			string_view{}
	};

	const bool allow
	{
		acl(ip_str)
	};

	log::debug
	{
		m::log, "%s requested key %s (%s)",
		loghead(client),
		key_id?: "*"_sv,
		allow? "ALLOWED": "DENIED",
	};

	if(!allow)
		return m::resource::response
		{
			client, http::FORBIDDEN
		};

	m::keys::get(my_host(), key_id, [&client]
	(const json::object &keys)
	{
		m::resource::response
		{
			client, http::OK, keys
		};
	});

	return {};
}

m::resource::method
method_get
{
	server_resource, "GET", handle_get
};
