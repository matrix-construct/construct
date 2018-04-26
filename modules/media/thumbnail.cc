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
		download(server, file)
	};

	return get__thumbnail_local(client, request, server, file, room_id);
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
