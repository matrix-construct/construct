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
	const string_view &since
	{
		request.has("since")?
			unquote(request["since"]):
			request.query["since"]
	};

	if(since && !valid(m::id::ROOM, since))
		throw m::BAD_REQUEST
		{
			"Invalid since token for this server."
		};

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
		request.has("limit")?
			uint8_t(request.at<ushort>("limit")):
			uint8_t(request.query.get<ushort>("limit", 16U))
	};

	const bool include_all_networks
	{
		request.get<bool>("include_all_networks", false)
	};

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher(), size_t(flush_hiwat)
	};

	size_t count{0};
	m::room::id::buf prev_batch_buf;
	m::room::id::buf next_batch_buf;
	json::stack::object top{out};
	{
		json::stack::member chunk_m{top, "chunk"};
		json::stack::array chunk{chunk_m};

		//TODO: better keying for search terms
		const string_view &key
		{
			since?
				since:
			server?
				server:
			search_term?
				search_term:
			my_host()
		};

		m::rooms::each_opts opts;
		opts.public_rooms = true;
		opts.key = key;
		opts.closure = [&](const m::room::id &room_id)
		{
			json::stack::object obj{chunk};
			m::rooms::summary_chunk(room_id, obj);

			if(!count && !empty(since))
				prev_batch_buf = room_id; //TODO: ???

			next_batch_buf = room_id;
			return ++count < limit;
		};

		m::rooms::for_each(opts);
	}

	json::stack::member
	{
		top, "total_room_count_estimate", json::value
		{
			ssize_t(m::rooms::count_public(server))
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

	return response;
}
