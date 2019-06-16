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
	"Client 2.1 :Versions"
};

resource
versions_resource
{
	"/_matrix/client/versions",
	{
		"(2.1) Gets the versions of the specification supported by the server."
	}
};

resource::response
get_versions(client &client,
             resource::request &request)
{
	static const json::object object
	{
		R"({"versions":["r0.3.0","r0.4.0","r0.5.0"]})"
	};

	return resource::response
	{
		client, object
	};
}

resource::method
getter
{
	versions_resource, "GET", get_versions
};
