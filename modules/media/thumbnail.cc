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

namespace ircd::m::media::thumbnail
{
	extern conf::item<bool> enable;
	extern conf::item<size_t> width_min;
	extern conf::item<size_t> width_max;
	extern conf::item<size_t> height_min;
	extern conf::item<size_t> height_max;
}

using namespace ircd::m::media::thumbnail; //TODO: XXX

decltype(ircd::m::media::thumbnail::enable)
ircd::m::media::thumbnail::enable
{
	{ "name",    "ircd.m.media.thumbnail.enable" },
	{ "default", true                            },
};

decltype(ircd::m::media::thumbnail::width_min)
ircd::m::media::thumbnail::width_min
{
	{ "name",     "ircd.m.media.thumbnail.width.min" },
	{ "default",  16L                                },
};

decltype(ircd::m::media::thumbnail::width_max)
ircd::m::media::thumbnail::width_max
{
	{ "name",     "ircd.m.media.thumbnail.width.max" },
	{ "default",  1536L                              },
};

decltype(ircd::m::media::thumbnail::height_min)
ircd::m::media::thumbnail::height_min
{
	{ "name",     "ircd.m.media.thumbnail.height.min" },
	{ "default",  16L                                 },
};

decltype(ircd::m::media::thumbnail::height_max)
ircd::m::media::thumbnail::height_max
{
	{ "name",     "ircd.m.media.thumbnail.height.max" },
	{ "default",  1536L                               },
};

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
                     const m::room &room,
                     const std::pair<size_t, size_t> &dimension);

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

	std::pair<size_t, size_t> dimension
	{
		request.query.get<size_t>("width", 0),
		request.query.get<size_t>("height", 0)
	};

	if(dimension.first)
	{
		dimension.first = std::max(dimension.first, size_t(width_min));
		dimension.first = std::min(dimension.first, size_t(width_max));
	}

	if(dimension.second)
	{
		dimension.second = std::max(dimension.second, size_t(height_min));
		dimension.second = std::min(dimension.second, size_t(height_max));
	}

	return get__thumbnail_local(client, request, server, file, room_id, dimension);
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
                     const m::room &room,
                     const std::pair<size_t, size_t> &dimension)
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

	//TODO: condition if magick available
	if(!enable || !dimension.first || !dimension.second)
		return resource::response
		{
			client, buf, content_type
		};

	magick::thumbnail
	{
		buf, dimension, [&client, &content_type]
		(const const_buffer &buf)
		{
			resource::response
			{
				client, buf, content_type
			};
		}
	};

	return {}; // responded from closure.
}
