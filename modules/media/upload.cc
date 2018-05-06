// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "media.h"

resource
upload_resource__legacy
{
	"/_matrix/media/v1/upload/",
	{
		"(11.7.1.1) upload (legacy compat)",
	}
};

resource
upload_resource
{
	"/_matrix/media/r0/upload/",
	{
		"(11.7.1.1) upload",
	}
};

resource::response
post__upload(client &client,
             const resource::request &request)
{
	const auto &content_type
	{
		request.head.content_type
	};

	const auto &server
	{
		my_host()
	};

	const auto &filename
	{
		request.query["filename"]
	};

	char randbuf[32];
	const auto randstr
	{
		rand::string(rand::dict::alpha, randbuf)
	};

	const m::room::id::buf room_id
	{
		file_room_id(server, randstr)
	};

	m::vm::copts vmopts;
	vmopts.history = false;
	const m::room room
	{
		room_id, &vmopts
	};

	create(room, request.user_id, "file");

	const unique_buffer<mutable_buffer> buf
	{
		request.head.content_length
	};

	copy(buf, request.content);
	client.content_consumed += read_all(*client.sock, buf);
	assert(client.content_consumed == request.head.content_length);

	const size_t written
	{
		write_file(room, request.user_id, buf, content_type)
	};

	char uribuf[256];
	const string_view content_uri
	{
		fmt::sprintf
		{
			uribuf, "mxc://%s/%s", server, randstr
		}
	};

	log::debug
	{
		"%s uploaded %zu bytes uri: `%s' file_room: %s :%s",
		request.user_id,
		request.head.content_length,
		content_uri,
		string_view{room.room_id},
		filename
	};

	return resource::response
	{
		client, http::CREATED, json::members
		{
			{ "content_uri", content_uri }
		}
	};
}

static const struct resource::method::opts
method_post_opts
{
	resource::method::REQUIRES_AUTH |
	resource::method::CONTENT_DISCRETION,

	-1s, // TODO: no coarse timer

	8_MiB //TODO: conf; (this is the payload max option)
};

static resource::method
method_post
{
	upload_resource, "POST", post__upload, method_post_opts
};

static resource::method
method_post__legacy
{
	upload_resource__legacy, "POST", post__upload, method_post_opts
};
