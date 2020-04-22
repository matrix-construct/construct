// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "rooms.h"

using namespace ircd;

m::resource::response
post__join(client &client,
           const m::resource::request &request,
           const m::room::id &room_id)
{
	static const size_t server_name_maxarg{16U};
	const auto server_name_count
	{
		std::min(request.query.count("server_name"), server_name_maxarg)
	};

	string_view server_name[server_name_maxarg];
	const unique_mutable_buffer server_name_buf
	{
		rfc3986::DOMAIN_BUFSIZE * server_name_count
	};

	const auto query_server_names
	{
		request.query.array(server_name_buf, "server_name", server_name)
	};

	//XXX ???
	const json::string &content_server_names
	{
		request["server_name"]
	};

	const auto &server_names
	{
		query_server_names
	};

	const json::string &third_party_signed
	{
		request["third_party_signed"]
	};

	const m::room room
	{
		room_id
	};

	const auto event_id
	{
		m::join(room, request.user_id, server_names)
	};

	return m::resource::response
	{
		client, json::members
		{
			{ "room_id", room_id }
		}
	};
}
