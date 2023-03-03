// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::admin
{
	static void make_deps(json::stack &deps);
	static resource::response handle_get(client &, const resource::request &);

	extern resource::method get_method;
	extern resource server_version_resource;
};

ircd::mapi::header
IRCD_MODULE
{
	"Admin (undocumented) :Server Version"
};

decltype(ircd::m::admin::server_version_resource)
ircd::m::admin::server_version_resource
{
	"/_synapse/admin/v1/server_version",
	{
		"(Synapse) Admin Server Version"
	}
};

decltype(ircd::m::admin::get_method)
ircd::m::admin::get_method
{
	server_version_resource, "GET", handle_get,
	{
		get_method.REQUIRES_OPER
	}
};

ircd::m::resource::response
ircd::m::admin::handle_get(client &client,
                           const resource::request &request)
{
	const unique_mutable_buffer buf
	{
		8_KiB
	};

	json::stack deps
	{
		buf
	};

	make_deps(deps);
	return m::resource::response
	{
		client, json::members
		{
			{ "server_name",     BRANDING_NAME     },
			{ "server_version",  BRANDING_VERSION  },
			{ "package",
			{
				{ "name",          PACKAGE_NAME        },
				{ "version",       PACKAGE_VERSION     },
				{ "string",        PACKAGE_STRING      },
				{ "tarname",       PACKAGE_TARNAME     },
			}},
			{ "build",
			{
				{ "version",       RB_VERSION          },
				{ "branch",        RB_VERSION_BRANCH   },
				{ "tag",           RB_VERSION_TAG      },
				{ "commit",        RB_VERSION_COMMIT   },
				{ "date",          RB_DATE_CONFIGURED  },
				{ "time",          RB_TIME_CONFIGURED  },
			}},
			{ "info",
			{
				{ "name",          info::name          },
				{ "version",       info::version       },
				{ "tag",           info::tag           },
				{ "branch",        info::branch        },
				{ "commit",        info::commit        },
				{ "configured",    info::configured    },
				{ "compiled",      info::compiled      },
				{ "compiler",      info::compiler      },
				{ "startup",       info::startup       },
				{ "kernel",        info::kernel_name   },
				{ "user_agent",    info::user_agent    },
				{ "server_agent",  info::server_agent  },
			}},
			{ "deps", deps.completed() },
		}
	};
}

void
ircd::m::admin::make_deps(json::stack &deps)
{
	json::stack::array array
	{
		deps
	};

	for(const auto &version : info::versions::list)
	{
		json::stack::object dep
		{
			array
		};

		json::stack::member
		{
			dep, "name", version->name
		};

		json::stack::member
		{
			dep, "type", json::value
			{
				version->type == version->API? "API":
				version->type == version->ABI? "ABI":
				                               "???"
			}
		};

		json::stack::member
		{
			dep, "monotonic", json::value
			{
				version->monotonic
			}
		};

		char buf[32];
		json::stack::member
		{
			dep, "semantic", string_view
			{
				fmt::sprintf
				{
					buf, "%ld.%ld.%ld",
					version->semantic[0],
					version->semantic[1],
					version->semantic[2],
				}
			}
		};

		json::stack::member
		{
			dep, "string", version->string
		};
	}
}
