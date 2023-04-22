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
	"Federation 14.1 :Public Rooms"
};

m::resource
publicrooms_resource
{
	"/_matrix/federation/v1/publicRooms",
	{
		"(14.1) Gets all the public rooms for the homeserver. This should not return "
		"rooms that are listed on another homeserver's directory, just those listed on "
		"the receiving homeserver's directory. "
	}
};

conf::item<size_t>
flush_hiwat
{
	{ "name",     "ircd.federation.publicrooms.flush.hiwat" },
	{ "default",  16384L                                    },
};

static m::resource::response
handle_get(client &,
           const m::resource::request &);

m::resource::method
get_method
{
	publicrooms_resource, "GET", handle_get,
	{
		get_method.VERIFY_ORIGIN
	}
};

m::resource::method
post_method
{
	publicrooms_resource, "POST", handle_get,
	{
		post_method.VERIFY_ORIGIN
	}
};

m::resource::response
handle_get(client &client,
           const m::resource::request &request)
{
	char sincebuf[m::room::id::buf::SIZE];
	const json::string &since
	{
		request.query["since"]?
			url::decode(sincebuf, request.query["since"]):
			request["since"]
	};

	if(since && !valid(m::id::ROOM, since))
		throw m::BAD_REQUEST
		{
			"Invalid since token for this server."
		};

	const uint8_t limit
	{
		request.has("limit")?
			uint8_t(request.at<ushort>("limit")):
			uint8_t(request.query.get<ushort>("limit", 255U))
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

	m::resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher(), size_t(flush_hiwat)
	};

	m::rooms::opts opts;
	opts.summary = true;
	opts.join_rule = "public";
	opts.server = my_host();
	opts.lower_bound = true;
	opts.room_id = since;
	opts.search_term = search_term;
	opts.request_node_id = request.node_id;
	opts.room_type = json::string(filter["room_type"]);

	size_t count{0};
	m::room::id::buf prev_batch_buf;
	m::room::id::buf next_batch_buf;
	json::stack::object top{out};
	{
		json::stack::array chunk
		{
			top, "chunk"
		};

		m::rooms::for_each(opts, [&]
		(const m::room::id &room_id)
		{
			json::stack::object obj
			{
				chunk
			};

			m::rooms::summary::get(obj, room_id);
			next_batch_buf = room_id;
			return ++count < limit;
		});
	}

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

	return response;
}
