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

resource
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

static resource::response
handle_get(client &,
           const resource::request &);

resource::method
get_method
{
	publicrooms_resource, "GET", handle_get,
	{
		get_method.VERIFY_ORIGIN
	}
};

resource::response
handle_get(client &client,
           const resource::request &request)
{
	const string_view &since
	{
		request.query["since"]
	};

	if(since && !valid(m::id::ROOM, since))
		throw m::BAD_REQUEST
		{
			"Invalid since token for this server."
		};

	if(since && m::room::id(since).host() != my_host())
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
		json::stack::array chunk
		{
			top, "chunk"
		};

		const string_view &key
		{
			since?: my_host()
		};

		m::rooms::for_each_public(key, [&]
		(const m::room::id &room_id)
		{
			json::stack::object obj{chunk};
			m::rooms::summary_chunk(room_id, obj);
			next_batch_buf = room_id;
			return ++count < limit;
		});
	}

	json::stack::member
	{
		top, "total_room_count_estimate", json::value
		{
			ssize_t(m::rooms::count_public(my_host()))
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
