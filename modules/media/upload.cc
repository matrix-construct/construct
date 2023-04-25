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

using namespace ircd;

m::resource
upload_resource
{
	"/_matrix/media/r0/upload/",
	{
		"(11.7.1.1) upload",
	}
};

m::resource::response
post__upload(client &client,
             const m::resource::request &request)
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
		rand::string(randbuf, rand::dict::alpha)
	};

	const m::media::mxc mxc
	{
		server, randstr
	};

	const m::room::id::buf room_id
	{
		m::media::file::room_id(mxc)
	};

	m::vm::copts vmopts;
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
	client.content_consumed += read_all(*client.sock, buf + client.content_consumed);
	assert(client.content_consumed == request.head.content_length);

	const size_t written
	{
		m::media::file::write(room, request.user_id, buf, content_type, filename)
	};

	char uribuf[256];
	const string_view content_uri
	{
		mxc.uri(uribuf)
	};

	log::debug
	{
		m::media::log, "%s uploaded %zu bytes uri: `%s' file_room %s :%s",
		request.user_id,
		request.head.content_length,
		content_uri,
		string_view{room.room_id},
		filename,
	};

	return m::resource::response
	{
		client, http::OK, json::members
		{
			{ "content_uri", content_uri }
		}
	};
}

static const struct m::resource::method::opts
method_post_opts
{
	m::resource::method::REQUIRES_AUTH |
	m::resource::method::CONTENT_DISCRETION,

	-1s, // TODO: no coarse timer

	8_MiB //TODO: conf; (this is the payload max option)
};

static m::resource::method
method_post
{
	upload_resource, "POST", post__upload, method_post_opts
};
