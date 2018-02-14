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
account_whoami
{
	"/_matrix/client/r0/account/whoami",
	{
		"(3.6.1) Gets information about the owner of a given access token."
	}
};

resource::response
get__whoami(client &client,
            const resource::request &request)
{
	return resource::response
	{
		client, json::members
		{
			{ "user_id", request.user_id }
		}
	};
}

resource::method
get_whoami
{
	account_whoami, "GET", get__whoami,
	{
		get_whoami.REQUIRES_AUTH
	}
};
