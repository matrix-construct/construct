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
	"Client 7.5 :Public Rooms"
};

const ircd::m::room::id::buf
public_room_id
{
    "public", ircd::my_host()
};

m::room public_
{
	public_room_id
};

resource
publicrooms_resource
{
	"/_matrix/client/r0/publicRooms",
	{
		"(7.5) Lists the public rooms on the server. "
	}
};

static resource::response
post__publicrooms_since(client &client,
                        const resource::request &request,
                        const string_view &since);

static resource::response
post__publicrooms_remote(client &client,
                         const resource::request &request,
                         const string_view &server,
                         const string_view &search,
                         const size_t &limit);

resource::response
get__publicrooms(client &client,
                 const resource::request &request)
{
	const auto &server
	{
		request.query["server"]
	};

	const auto &since
	{
		request.query["since"]
	};

	const uint8_t limit
	{
		request.query["limit"]?
			uint8_t(lex_cast<ushort>(request.query["limit"])):
			uint8_t(16U)
	};

	std::vector<json::value> chunk;
	const string_view next_batch;
	const string_view prev_batch;
	const int64_t total_room_count_estimate{0};

	return resource::response
	{
		client, json::members
		{
			{ "chunk",                      { chunk.data(), chunk.size() } },
			{ "next_batch",                 next_batch                     },
			{ "prev_batch",                 prev_batch                     },
			{ "total_room_count_estimate",  total_room_count_estimate      },
		}
	};
}

resource::method
get_method
{
	publicrooms_resource, "GET", get__publicrooms
};

resource::response
post__publicrooms(client &client,
                  const resource::request &request)
{
	const string_view &since
	{
		unquote(request["since"])
	};

	if(!empty(since))
		return post__publicrooms_since(client, request, since);

	const auto &server
	{
		request.query["server"]
	};

	const json::object &filter
	{
		request["filter"]
	};

	const string_view &search_term
	{
		unquote(filter["generic_search_term"])
	};

	const uint8_t limit
	{
		uint8_t(request.get<ushort>("limit", 16U))
	};

	const bool include_all_networks
	{
		request.get<bool>("include_all_networks", false)
	};

	if(server && server != my_host())
		return post__publicrooms_remote(client, request, server, search_term, limit);

	std::vector<json::value> chunk;
	const string_view next_batch;
	const string_view prev_batch;
	const int64_t total_room_count_estimate{0};

	return resource::response
	{
		client, json::members
		{
			{ "chunk",                      { chunk.data(), chunk.size() } },
			{ "next_batch",                 next_batch                     },
			{ "prev_batch",                 prev_batch                     },
			{ "total_room_count_estimate",  total_room_count_estimate      },
		}
	};
}

resource::method
get_method
{
	publicrooms_resource, "GET", get__publicrooms
};

static void
_create_public_room(const m::event &)
{
	m::create(public_room_id, m::me.user_id);
}

/// Create the public rooms room at the appropriate time on startup.
/// The startup event chosen here is when @ircd joins the !ircd room,
/// which is a fundamental notification toward the end of init.
const m::hook<>
_create_public_hook
{
	_create_public_room,
	{
		{ "_site",       "vm.notify"      },
		{ "room_id",     "!ircd"          },
		{ "type",        "m.room.create"  },
	}
};
