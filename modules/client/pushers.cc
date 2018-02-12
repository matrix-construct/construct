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

mapi::header IRCD_MODULE
{
	"registers the resource 'client/pushers'"
};

ircd::resource pushers_resource
{
	"/_matrix/client/r0/pushers/", resource::opts
	{
		"pushers",
		resource::DIRECTORY,
	}
};

resource::response
get_pushers(client &client,
             const resource::request &request)
{
	return resource::response
	{
		client, http::OK
	};
}

resource::method method_get
{
	pushers_resource, "GET", get_pushers,
	{
		method_get.REQUIRES_AUTH
	}
};
