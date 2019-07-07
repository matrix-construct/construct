// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::client_versions
{
	static void append_unstable_features(client &, resource::request &, json::stack &);
	static void append_versions(client &, resource::request &, json::stack &);
	static resource::response get(client &, resource::request &);

	extern conf::item<bool> m_lazy_load_members;
	extern conf::item<std::string> versions;
	extern resource::method method_get;
	extern resource resource;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client 2.1 :Versions"
};

decltype(ircd::m::client_versions::resource)
ircd::m::client_versions::resource
{
	"/_matrix/client/versions",
	{
		"(2.1) Gets the versions of the specification supported by the server."
	}
};

decltype(ircd::m::client_versions::method_get)
ircd::m::client_versions::method_get
{
	resource, "GET", get
};

ircd::resource::response
ircd::m::client_versions::get(client &client,
                              resource::request &request)
{
	char buf[512];
	json::stack out{buf};
	{
		json::stack::object top{out};
		append_versions(client, request, out);
		append_unstable_features(client, request, out);
	}

	return resource::response
	{
		client, json::object
		{
			out.completed()
		}
	};
}

/// Note this conf item doesn't persist to and from the database, which means
/// it assumes its default value on every startup.
decltype(ircd::m::client_versions::versions)
ircd::m::client_versions::versions
{
	{ "name",     "ircd.m.client.versions.versions" },
	{ "default",  "r0.3.0 r0.4.0 r0.5.0"            },
	{ "persist",  false                             },
};

void
ircd::m::client_versions::append_versions(client &client,
                                          resource::request &request,
                                          json::stack &out)
{
	json::stack::array array
	{
		out, "versions"
	};

	const string_view &list
	{
		versions
	};

	tokens(list, ' ', [&array]
	(const string_view &version)
	{
		array.append(version);
	});
}

decltype(ircd::m::client_versions::m_lazy_load_members)
ircd::m::client_versions::m_lazy_load_members
{
	{ "name",     "ircd.m.client.versions.m_lazy_load_members" },
	{ "default",  true,                                        },
};

void
ircd::m::client_versions::append_unstable_features(client &client,
                                                   resource::request &request,
                                                   json::stack &out)
{
	json::stack::object object
	{
		out, "unstable_features"
	};

	// m.lazy_load_members
	json::stack::member
	{
		out, "m.lazy_load_members", json::value
		{
			bool(m_lazy_load_members)
		}
	};
}
