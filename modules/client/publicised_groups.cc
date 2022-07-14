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
	"Client (unspecified) :Publicised Groups",
};

m::resource
publicised_groups_resource
{
	"/_matrix/client/r0/publicised_groups",
	{
		"(7.5) Lists the public rooms on the server. "
	}
};

m::resource::response
get__publicised_groups(client &client,
                       const m::resource::request &request)
{
	return resource::response
	{
		client, json::members
		{
			{ "users", json::empty_array }
		}
	};
}

m::resource::method
get_method
{
	publicised_groups_resource, "GET", get__publicised_groups
};

m::resource::response
post__publicised_groups(client &client,
                        const m::resource::request &request)
{
	const json::array &user_ids
	{
		request["user_ids"]
	};

	return resource::response
	{
		client, json::members
		{
			{ "users", user_ids }
		}
	};
}

m::resource::method
post_method
{
	publicised_groups_resource, "POST", post__publicised_groups
};
