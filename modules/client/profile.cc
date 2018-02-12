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
	"registers the resource 'client/profile'"
};

ircd::resource profile_resource
{
	"/_matrix/client/r0/profile/", resource::opts
	{
		"profile",
		resource::DIRECTORY,
	}
};

resource::response
put_profile(client &client,
             const resource::request &request)
{
	return resource::response
	{
		client, http::OK
	};
}

resource::method method_put
{
	profile_resource, "PUT", put_profile,
	{
		method_put.REQUIRES_AUTH
	}
};
