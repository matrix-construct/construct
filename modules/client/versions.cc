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

	extern conf::item<bool> e2ee_forced_public;
	extern conf::item<bool> e2ee_forced_private;
	extern conf::item<bool> e2ee_forced_trusted_private;
	extern conf::item<bool> m_require_identity_server;
	extern conf::item<bool> org_matrix_e2e_cross_signing;
	extern conf::item<bool> org_matrix_label_based_filtering;
	extern conf::item<bool> m_lazy_load_members;
	extern conf::item<std::string> versions;
	extern const string_view versions_default;
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

decltype(ircd::m::client_versions::versions_default)
ircd::m::client_versions::versions_default
{
	"r0.0.1"
	" r0.1.0"
	" r0.2.0"
	" r0.3.0"
	" r0.4.0"
	" r0.5.0"
	" r0.6.0"
	" r0.6.1"
	" v1.1"
	" v1.2"
	" v1.3"
	" v1.4"
	" v1.5"
	" v1.6"
};

/// Note this conf item doesn't persist to and from the database, which means
/// it assumes its default value on every startup.
decltype(ircd::m::client_versions::versions)
ircd::m::client_versions::versions
{
	{ "name",     "ircd.m.client.versions.versions"  },
	{ "persist",  false                              },
	{ "default",  versions_default                   },
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

decltype(ircd::m::client_versions::org_matrix_label_based_filtering)
ircd::m::client_versions::org_matrix_label_based_filtering
{
	{ "name",     "ircd.m.client.versions.org_matrix_label_based_filtering" },
	{ "default",  true,                                                     },
};

decltype(ircd::m::client_versions::org_matrix_e2e_cross_signing)
ircd::m::client_versions::org_matrix_e2e_cross_signing
{
	{ "name",     "ircd.m.client.versions.org_matrix_e2e_cross_signing" },
	{ "default",  true,                                                 },
};

decltype(ircd::m::client_versions::m_require_identity_server)
ircd::m::client_versions::m_require_identity_server
{
	{ "name",     "ircd.m.client.versions.m_require_identity_server" },
	{ "default",  false,                                             },
};

decltype(ircd::m::client_versions::e2ee_forced_public)
ircd::m::client_versions::e2ee_forced_public
{
	{ "name",     "ircd.m.client.versions.e2ee_forced.public" },
	{ "default",  false,                                      },
};

decltype(ircd::m::client_versions::e2ee_forced_private)
ircd::m::client_versions::e2ee_forced_private
{
	{ "name",     "ircd.m.client.versions.e2ee_forced.private" },
	{ "default",  false,                                       },
};

decltype(ircd::m::client_versions::e2ee_forced_trusted_private)
ircd::m::client_versions::e2ee_forced_trusted_private
{
	{ "name",     "ircd.m.client.versions.e2ee_forced.trusted_private" },
	{ "default",  false,                                               },
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

	// org.matrix.label_based_filtering
	json::stack::member
	{
		out, "org.matrix.label_based_filtering", json::value
		{
			bool(org_matrix_label_based_filtering)
		}
	};

	// org.matrix.e2e_cross_signing
	json::stack::member
	{
		out, "org.matrix.e2e_cross_signing", json::value
		{
			bool(org_matrix_e2e_cross_signing)
		}
	};

	// m.require_identity_server
	json::stack::member
	{
		out, "m.require_identity_server", json::value
		{
			bool(m_require_identity_server)
		}
	};

	// e2ee_forced.public
	json::stack::member
	{
		out, "io.element.e2ee_forced.public", json::value
		{
			bool(e2ee_forced_public)
		}
	};

	// e2ee_forced.private
	json::stack::member
	{
		out, "io.element.e2ee_forced.private", json::value
		{
			bool(e2ee_forced_private)
		}
	};

	// e2ee_forced.trusted_private
	json::stack::member
	{
		out, "io.element.e2ee_forced.trusted_private", json::value
		{
			bool(e2ee_forced_trusted_private)
		}
	};
}
