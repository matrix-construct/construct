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
	"Client (unstable) (no-section) :Third party protocols"
};

m::resource
protocols_resource
{
	"/_matrix/client/r0/thirdparty/protocols",
	{
		"(no-section) Unstable thirdparty protocols support"
	}
};

m::resource::response
get__protocols(client &client,
               const m::resource::request &request)
{
	return resource::response
	{
		client, http::OK
	};
}

m::resource::method
get_method
{
	protocols_resource, "GET", get__protocols
};
