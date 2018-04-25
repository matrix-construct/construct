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
download_resource
{
	"/_matrix/media/r0/download/",
	{
		"(11.7.1.2) download",
		resource::DIRECTORY,
	}
};

resource
download_resource__legacy
{
	"/_matrix/media/v1/download/",
	{
		"(11.7.1.2) download (legacy)",
		resource::DIRECTORY,
	}
};

static resource::response
get__download_local(client &client,
                    const resource::request &request,
                    const string_view &server,
                    const string_view &file,
                    const m::room &room);

static resource::response
get__download(client &client,
              const resource::request &request)
{
	if(request.parv.size() < 2)
		throw http::error
		{
			http::MULTIPLE_CHOICES, "/ download / domain / file"
		};

	const auto &server
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

	if(m::exists(room))
		return get__download_local(client, request, server, file, room);

	throw m::NOT_FOUND
	{
		"Media not found"
	};
}

static resource::response
get__download_local(client &client,
                    const resource::request &request,
                    const string_view &server,
                    const string_view &file,
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
	resource::response
	{
		client, http::OK, content_type, file_size
	};

	size_t sent{0}, read;
	read = read_each_block(room, [&client, &sent]
	(const string_view &block)
	{
		sent += write_all(*client.sock, block);
	});

	if(unlikely(read != file_size)) log::error
	{
		media_log, "File %s/%s [%s] size mismatch: expected %zu got %zu",
		server,
		file,
		string_view{room.room_id},
		file_size,
		read
	};

	// Have to kill client here after failing content length expectation.
	if(unlikely(read != file_size))
		client.close(net::dc::RST, net::close_ignore);

	return {};
}

static resource::method
method_get
{
	download_resource, "GET", get__download
};

static resource::method
method_get__legacy
{
	download_resource__legacy, "GET", get__download
};
