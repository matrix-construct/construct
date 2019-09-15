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

resource
publicrooms_resource
{
	"/_matrix/client/r0/publicRooms",
	{
		"(7.5) Lists the public rooms on the server. "
	}
};

conf::item<size_t>
flush_hiwat
{
	{ "name",     "ircd.client.publicrooms.flush.hiwat" },
	{ "default",  16384L                                },
};

static resource::response
get__publicrooms(client &,
                 const resource::request &);

resource::method
post_method
{
	publicrooms_resource, "POST", get__publicrooms
};

resource::method
get_method
{
	publicrooms_resource, "GET", get__publicrooms
};

resource::response
get__publicrooms(client &client,
                 const resource::request &request)
{
	char since_buf[m::room::id::buf::SIZE];
	const string_view &since
	{
		request.has("since")?
			unquote(request["since"]):
			url::decode(since_buf, request.query["since"])
	};

	if(since && !valid(m::id::ROOM, since))
		throw m::BAD_REQUEST
		{
			"Invalid since token for this server."
		};

	char server_buf[256];
	string_view server
	{
		url::decode(server_buf, request.query["server"])
	};

	const uint8_t limit
	{
		request.has("limit")?
			uint8_t(request.at<ushort>("limit")):
			uint8_t(request.query.get<ushort>("limit", 16U))
	};

	const bool include_all_networks
	{
		request.get<bool>("include_all_networks", false)
	};

	const json::object &filter
	{
		request["filter"]
	};

	const json::string &search_term
	{
		filter["generic_search_term"]
	};

	if(!server && m::valid(m::id::ROOM_ALIAS, search_term))
		server = m::id::room_alias(search_term).host();

	if(server && !my_host(server)) try
	{
		m::rooms::summary::fetch
		{
			server, since, limit
		};
	}
	catch(const std::exception &e)
	{
		log::error
		{
			m::log, "Failed to fetch public rooms from '%s' :%s",
			server,
			e.what()
		};
	}

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher(), size_t(flush_hiwat)
	};

	m::rooms::opts opts;
	opts.join_rule = "public";
	opts.summary = true;
	opts.search_term = search_term;
	opts.lower_bound = true;
	opts.room_id = since;

	if(m::valid(m::id::USER, search_term))
		opts.user_id = search_term;

	opts.room_alias =
		startswith(search_term, m::id::ROOM_ALIAS)?
			string_view{search_term}:
			string_view{};

	opts.server =
		server?
			server:
		opts.room_alias || opts.user_id?
			string_view{}:
			my_host();

	log::debug
	{
		m::log, "public rooms query server[%s] search[%s] filter:%s"
		" user_id:%b room_alias:%b allnet:%b since:%s",
		opts.server,
		opts.search_term,
		string_view{filter},
		bool(opts.user_id),
		bool(opts.room_alias),
		include_all_networks,
		since,
	};

	size_t count{0};
	m::room::id::buf prev_batch_buf;
	m::room::id::buf next_batch_buf;
	json::stack::object top{out};
	{
		json::stack::member chunk_m{top, "chunk"};
		json::stack::array chunk{chunk_m};
		m::rooms::for_each(opts, [&](const m::room::id &room_id)
		{
			json::stack::object obj{chunk};
			m::rooms::summary::get(obj, room_id);
			if(!count && !empty(since))
				prev_batch_buf = room_id; //TODO: ???

			next_batch_buf = room_id;
			return ++count < limit;
		});
	}

	// To count the total we clear the since token, otherwise the count
	// will be the remainder.
	opts.room_id = {};
	json::stack::member
	{
		top, "total_room_count_estimate", json::value
		{
			ssize_t(m::rooms::count(opts))
		}
	};

	if(prev_batch_buf)
		json::stack::member
		{
			top, "prev_batch", prev_batch_buf
		};

	if(count >= limit)
		json::stack::member
		{
			top, "next_batch", next_batch_buf
		};

	return std::move(response);
}
