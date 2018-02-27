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
	"Client-Sever 11.7.1.4 :Media thumbnails"
};

resource
thumbnail_resource__legacy
{
	"/_matrix/media/v1/thumbnail/",
	{
		"Media thumbnails (legacy version)",
		resource::DIRECTORY,
	}
};

resource
thumbnail_resource
{
	"/_matrix/media/r0/thumbnail/",
	{
		"Media thumbnails",
		resource::DIRECTORY,
	}
};

static resource::response
get__thumbnail_remote(client &client,
                      const resource::request &request,
                      const string_view &server,
                      const string_view &file);

resource::response
get__thumbnail(client &client,
               const resource::request &request)
{
	if(request.parv.size() < 1)
		throw http::error
		{
			http::MULTIPLE_CHOICES, "Server name parameter required"
		};

	if(request.parv.size() < 2)
		throw http::error
		{
			http::MULTIPLE_CHOICES, "Media ID parameter required"
		};

	const auto &server
	{
		request.parv[0]
	};

	const auto &file
	{
		request.parv[1]
	};

	//TODO: cache check

	if(!my_host(server))
		return get__thumbnail_remote(client, request, server, file);

	throw m::NOT_FOUND{}; //TODO: X

	const string_view data
	{

	};

	char mime_type_buf[64];
	const string_view content_type
	{
		magic::mime(mime_type_buf, data)
	};

	return resource::response
	{
		client, string_view{data}, content_type
	};
}

resource::method
method_get__legacy
{
	thumbnail_resource__legacy, "GET", get__thumbnail
};

resource::method
method_get
{
	thumbnail_resource, "GET", get__thumbnail
};

static resource::response
get__thumbnail_remote(client &client,
                      const resource::request &request,
                      const string_view &hostname,
                      const string_view &mediaid)
{
	const net::hostport remote
	{
		hostname
	};

	char buf[6_KiB];
	window_buffer wb{buf};
	thread_local char uri[2_KiB];
	http::request
	{
		wb, hostname, "GET", fmt::sprintf
		{
			uri, "/_matrix/media/r0/download/%s/%s", hostname, mediaid
		}
	};

	const const_buffer out_head
	{
		wb.completed()
	};

	// Remaining space in buffer is used for received head
	const mutable_buffer in_head
	{
		buf + size(out_head), sizeof(buf) - size(out_head)
	};

	// Null content buffer will cause dynamic allocation internally.
	const mutable_buffer in_content{};

	struct server::request::opts opts;
	server::request remote_request
	{
		remote, { out_head }, { in_head, in_content }, &opts
	};

	if(remote_request.wait(seconds(5)) == ctx::future_status::timeout) //TODO: conf
		throw http::error
		{
			http::REQUEST_TIMEOUT
		};

	const auto &code
	{
		remote_request.get()
	};

	//TODO: cache add

	char mime_type_buf[64];
	const string_view content_type
	{
		magic::mime(mime_type_buf, remote_request.in.content)
	};

	parse::buffer pb{remote_request.in.head};
	parse::capstan pc{pb};
	pc.read += size(remote_request.in.head);
	const http::response::head head{pc};
	if(content_type != head.content_type)
		log::warning
		{
			"Server %s claims thumbnail %s is '%s' but we think it is '%s'",
			hostname,
			mediaid,
			head.content_type,
			content_type
		};

	return resource::response
	{
		client, remote_request.in.content, content_type
	};
}
