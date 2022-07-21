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

m::resource
account_3pid
{
	"/_matrix/client/r0/account/3pid",
	{
		"(3.5) Adding Account Administrative Contact Information"
	}
};

m::resource::response
get__3pid(client &client,
          const m::resource::request &request)
{
	std::vector<json::value> vec;
	json::value threepids
	{
		vec.data(), vec.size()
	};

	return m::resource::response
	{
		client, json::members
		{
			{ "threepids", threepids }
		}
	};
}

m::resource::method
get_3pid
{
	account_3pid, "GET", get__3pid,
	{
		get_3pid.REQUIRES_AUTH
	}
};

m::resource::response
post__3pid(client &client,
          const m::resource::request &request)
{
	return m::resource::response
	{
		client, http::OK
	};
}

m::resource::method
post_3pid
{
	account_3pid, "POST", post__3pid,
	{
		post_3pid.REQUIRES_AUTH |
		post_3pid.RATE_LIMITED // revisit this? some of these require rate limiting, some don't
	}
};
