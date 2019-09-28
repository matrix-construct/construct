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

m::resource::response
handle_get(client &client,
           const m::resource::request &request)
{
	char key_id_buf[256];
	const auto key_id
	{
		request.parv.size() > 0?
			url::decode(key_id_buf, request.parv[0]):
			string_view{}
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
