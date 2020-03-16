// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::pushrules
{
	static resource::response handle_delete(client &, const resource::request &);
	static resource::response handle_put(client &, const resource::request &);
	static resource::response handle_get(client &, const resource::request &);
	extern resource::method method_get;
	extern resource::method method_put;
	extern resource::method method_delete;
	extern resource resource;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client 0.6.0-13.13.1.6 :Push Rules API"
};

decltype(ircd::m::pushrules::resource)
ircd::m::pushrules::resource
{
	"/_matrix/client/r0/pushrules",
	{
		"(11.12.1.5) Clients can retrieve, add, modify and remove push"
		" rules globally or per-device"
		,resource::DIRECTORY
	}
};

decltype(ircd::m::pushrules::method_get)
ircd::m::pushrules::method_get
{
	resource, "GET", handle_get,
	{
		method_get.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::pushrules::handle_get(client &client,
                               const resource::request &request)
{
	char buf[3][64];
	const auto &scope
	{
		request.parv.size() > 0?
			url::decode(buf[0], request.parv[0]):
			string_view{},
	};

	const auto &kind
	{
		request.parv.size() > 1?
			url::decode(buf[1], request.parv[1]):
			string_view{},
	};

	const auto &ruleid
	{
		request.parv.size() > 2?
			url::decode(buf[2], request.parv[2]):
			string_view{},
	};


	return resource::response
	{
		client, json::members
		{
			{ "global", json::members
			{
				{ "content",     json::array{} },
				{ "override",    json::array{} },
				{ "room",        json::array{} },
				{ "sender",      json::array{} },
				{ "underride",   json::array{} },
			}}
		}
	};
}

decltype(ircd::m::pushrules::method_put)
ircd::m::pushrules::method_put
{
	resource, "PUT", handle_put,
	{
		method_put.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::pushrules::handle_put(client &client,
                               const resource::request &request)
{
	char buf[3][64];
	const auto &scope
	{
		request.parv.size() > 0?
			url::decode(buf[0], request.parv[0]):
			string_view{},
	};

	const auto &kind
	{
		request.parv.size() > 1?
			url::decode(buf[1], request.parv[1]):
			string_view{},
	};

	const auto &ruleid
	{
		request.parv.size() > 2?
			url::decode(buf[2], request.parv[2]):
			string_view{},
	};

	return resource::response
	{
		client, json::object{}
	};
}

decltype(ircd::m::pushrules::method_delete)
ircd::m::pushrules::method_delete
{
	resource, "DELETE", handle_delete,
	{
		method_delete.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::pushrules::handle_delete(client &client,
                                  const resource::request &request)
{
	char buf[3][64];
	const auto &scope
	{
		request.parv.size() > 0?
			url::decode(buf[0], request.parv[0]):
			string_view{},
	};

	const auto &kind
	{
		request.parv.size() > 1?
			url::decode(buf[1], request.parv[1]):
			string_view{},
	};

	const auto &ruleid
	{
		request.parv.size() > 2?
			url::decode(buf[2], request.parv[2]):
			string_view{},
	};

	return resource::response
	{
		client, json::object{}
	};
}
