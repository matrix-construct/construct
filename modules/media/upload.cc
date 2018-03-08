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

mapi::header
IRCD_MODULE
{
	"11.7 :Content repository (upload)"
};

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

	const auto filename
	{
		request.query["filename"]
	};

	char pathbuf[32];
	const auto path
	{
		rand::string(rand::dict::alpha, pathbuf)
	};

	sha256 hash;
	size_t offset{0};
	while(offset < size(request.content))
	{
		const string_view pending
		{
			data(request.content) + offset, size(request.content) - offset
		};

		const auto appended
		{
			fs::append(path, pending, offset)
		};

		hash.update(appended);
		offset += size(appended);
	}
	assert(offset == client.content_consumed);

	char buffer[4_KiB];
	while(client.content_consumed < request.head.content_length)
	{
		const size_t remain
		{
			request.head.content_length - client.content_consumed
		};

		const mutable_buffer buf
		{
			buffer, std::min(remain, sizeof(buf))
		};

		const string_view read
		{
			data(buf), read_few(*client.sock, buf)
		};

		client.content_consumed += size(read); do
		{
			const auto appended
			{
				fs::append(path, read, offset)
			};

			hash.update(appended);
			offset += size(appended);
		}
		while(offset < client.content_consumed);
		assert(offset == client.content_consumed);
	}
	assert(offset == request.head.content_length);

	char hashbuf[32];
	hash.digest(hashbuf);

	char b58buf[64];
	const auto new_path
	{
		b58encode(b58buf, hashbuf)
	};

	fs::rename(path, new_path);

	char uribuf[256];
	const string_view content_uri
	{
		fmt::sprintf
		{
			uribuf, "mxc://%s/%s", my_host(), new_path
		}
	};

	return resource::response
	{
		client, http::CREATED, json::members
		{
			{ "content_uri", content_uri }
		}
	};
}

struct resource::method::opts
const method_post_opts
{
	resource::method::REQUIRES_AUTH |
	resource::method::CONTENT_DISCRETION,

	8_MiB //TODO: conf; (this is the payload max option)
};

resource::method
method_post
{
	upload_resource, "POST", post__upload, method_post_opts
};

resource::method
method_post__legacy
{
	upload_resource__legacy, "POST", post__upload, method_post_opts
};
