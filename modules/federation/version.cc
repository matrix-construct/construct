// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static resource::response federation_version__get(client &, const resource::request &);
	extern resource::method federation_version_get;
	extern resource federation_version;
}

ircd::mapi::header
IRCD_MODULE
{
	"Federation 2.2 :Version"
};

decltype(ircd::m::federation_version)
ircd::m::federation_version
{
	"/_matrix/federation/v1/version",
	{
		"(2.2) Get the implementation name and version of this homeserver",
	}
};

decltype(ircd::m::federation_version_get)
ircd::m::federation_version_get
{
	federation_version, "GET", federation_version__get
};

ircd::m::resource::response
ircd::m::federation_version__get(client &client,
                                 const resource::request &request)
{
	const json::members server
	{
		{ "name",     ircd::info::name              },
		{ "version",  ircd::info::version           },
		{ "branch",   ircd::info::branch            },
		{ "commit",   ircd::info::commit            },
		{ "compiler", ircd::info::compiler          },
		{ "kernel",   ircd::info::kernel_name       },
		{ "arch",     ircd::info::hardware::arch    },
	};

	return m::resource::response
	{
		client, json::members
		{
			{ "server", server }
		}
	};
}
