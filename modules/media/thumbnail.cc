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

	// Thumbnail doesn't require auth so if there is no user_id detected
	// then we download on behalf of @ircd.
	const m::user::id &user_id
	{
		request.user_id?
			m::user::id{request.user_id}:
			m::me.user_id
	};

	const m::room::id::buf room_id
	{
		download(server, file, user_id)
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
	static const m::event::fetch::opts fopts
	{
		m::event::keys::include {"content"}
	};

	const m::room::state state
	{
		room, &fopts
	};

	// Get the file's total size
	size_t file_size{0};
	state.get("ircd.file.stat", "size", [&file_size]
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

	state.get("ircd.file.stat", "type", [&type_buf, &content_type]
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

	const unique_buffer<mutable_buffer> buf
	{
		file_size
	};

	size_t copied(0);
	const auto sink{[&buf, &copied]
	(const const_buffer &block)
	{
		copied += copy(buf + copied, block);
	}};

	const size_t read_size
	{
		read_each_block(room, sink)
	};

	if(unlikely(read_size != file_size || file_size != copied))
		throw ircd::error
		{
			"File %s/%s [%s] size mismatch: expected %zu got %zu copied %zu",
			hostname,
			mediaid,
			string_view{room.room_id},
			file_size,
			read_size,
			copied
		};

	const auto width
	{
		request.query.get<size_t>("width", 0)
	};

	const auto height
	{
		request.query.get<size_t>("height", 0)
	};

	if(!width || !height || width > 1536 || height > 1536) // TODO: confs..
		return resource::response
		{
			client, buf, content_type
		};

	magick::thumbnail
	{
		buf, {width, height}, [&client, &content_type]
		(const const_buffer &buf)
		{
			resource::response
			{
				client, buf, content_type
			};
		}
	};

	return {};
}
