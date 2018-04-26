// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <boost/gil/image.hpp>
#include <boost/gil/typedefs.hpp>
#include <boost/gil/extension/io/jpeg_io.hpp>
// #include <boost/gil/extension/numeric/sampler.hpp>
// #include <boost/gil/extension/numeric/resample.hpp>

#include "media.h"

resource
thumbnail_resource__legacy
{
	"/_matrix/media/v1/thumbnail/",
	{
		"(11.7.1.4) thumbnails (legacy version)",
		resource::DIRECTORY,
	}
};

resource
thumbnail_resource
{
	"/_matrix/media/r0/thumbnail/",
	{
		"(11.7.1.4) thumbnails",
		resource::DIRECTORY,
	}
};

static resource::response
get__thumbnail_remote(client &client,
                      const resource::request &request,
                      const string_view &remote,
                      const string_view &server,
                      const string_view &file,
                      const m::room &room);

static resource::response
get__thumbnail_local(client &client,
                     const resource::request &request,
                     const string_view &server,
                     const string_view &file,
                     const m::room &room);

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

	auto &server
	{
		request.parv[0]
	};

	const auto &file
	{
		request.parv[1]
	};

	const m::room::id::buf room_id
	{
		file_room_id(server, file)
	};

	m::vm::opts::commit vmopts;
	vmopts.history = false;
	const m::room room
	{
		room_id, &vmopts
	};

	//TODO: ABA
	if(m::exists(room))
		return get__thumbnail_local(client, request, server, file, room);

	//TODO: XXX conf
	//TODO: XXX vector
	static const string_view secondary
	{
		"matrix.org"
	};

	const string_view &remote
	{
		my_host(server)?
			secondary:
			server
	};

	if(!my_host(remote))
	{
		//TODO: ABA TXN
		create(room, m::me.user_id, "file");
		return get__thumbnail_remote(client, request, remote, server, file, room);
	}

	throw m::NOT_FOUND
	{
		"Media not found"
	};
}

static resource::method
method_get__legacy
{
	thumbnail_resource__legacy, "GET", get__thumbnail
};

static resource::method
method_get
{
	thumbnail_resource, "GET", get__thumbnail
};

static resource::response
get__thumbnail_remote(client &client,
                      const resource::request &request,
                      const string_view &hostname,
                      const string_view &server,
                      const string_view &mediaid,
                      const m::room &room)
try
{
	const net::hostport remote
	{
		hostname
	};

	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	window_buffer wb{buf};
	thread_local char uri[4_KiB];
	http::request
	{
		wb, hostname, "GET", fmt::sprintf
		{
			uri, "/_matrix/media/r0/download/%s/%s", server, mediaid
		}
	};

	const const_buffer out_head
	{
		wb.completed()
	};

	// Remaining space in buffer is used for received head
	const mutable_buffer in_head
	{
		data(buf) + size(out_head), size(buf) - size(out_head)
	};

	//TODO: --- This should use the progress callback to build blocks

	// Null content buffer will cause dynamic allocation internally.
	const mutable_buffer in_content{};

	struct server::request::opts opts;
	server::request remote_request
	{
		remote, { out_head }, { in_head, in_content }, &opts
	};

	if(!remote_request.wait(seconds(10), std::nothrow))
		throw http::error
		{
			http::REQUEST_TIMEOUT
		};

	//TODO: ---

	const auto &code
	{
		remote_request.get()
	};

	char mime_type_buf[64];
	const string_view content_type
	{
		magic::mime(mime_type_buf, remote_request.in.content)
	};

	const size_t file_size
	{
		size(remote_request.in.content)
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

	const size_t written
	{
		write_file(room, remote_request.in.content, content_type)
	};

	return resource::response
	{
		client, remote_request.in.content, content_type
	};
}
catch(const ircd::server::unavailable &e)
{
	throw http::error
	{
		http::BAD_GATEWAY, e.what()
	};
}

static resource::response
get__thumbnail_local(client &client,
                     const resource::request &request,
                     const string_view &hostname,
                     const string_view &mediaid,
                     const m::room &room)
{
	// Get the file's total size
	size_t file_size{0};
	room.get("ircd.file.stat", "size", [&file_size]
	(const m::event &event)
	{
		file_size = at<"content"_>(event).get<size_t>("value");
	});

	// Get the MIME type
	char type_buf[64];
	string_view content_type
	{
		"application/octet-stream"
	};

	room.get("ircd.file.stat", "type", [&type_buf, &content_type]
	(const m::event &event)
	{
		const auto &value
		{
			unquote(at<"content"_>(event).at("value"))
		};

		content_type =
		{
			type_buf, copy(type_buf, value)
		};
	});

	// Send HTTP head to client
	const resource::response response
	{
		client, http::OK, content_type, file_size
	};

	size_t sent_size{0};
	const auto sink{[&client, &sent_size]
	(const const_buffer &block)
	{
		sent_size += client.write_all(block);
	}};

	const size_t read_size
	{
		read_each_block(room, sink)
	};

	if(unlikely(read_size != file_size)) log::error
	{
		media_log, "File %s/%s [%s] size mismatch: expected %zu got %zu",
		hostname,
		mediaid,
		string_view{room.room_id},
		file_size,
		read_size
	};

	// Have to kill client here after failing content length expectation.
	if(unlikely(read_size != file_size))
		client.close(net::dc::RST, net::close_ignore);

	assert(read_size == sent_size);
	return response;
}
