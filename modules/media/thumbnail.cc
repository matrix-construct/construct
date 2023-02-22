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

using namespace ircd::m::media::thumbnail; //TODO: XXX
using namespace ircd;

decltype(ircd::m::media::thumbnail::enable)
ircd::m::media::thumbnail::enable
{
	{ "name",    "ircd.m.media.thumbnail.enable" },
	{ "default", true                            },
};

decltype(ircd::m::media::thumbnail::enable_remote)
ircd::m::media::thumbnail::enable_remote
{
	{ "name",    "ircd.m.media.thumbnail.enable_remote" },
	{ "default", true                                   },
};

decltype(ircd::m::media::thumbnail::animation_enable)
ircd::m::media::thumbnail::animation_enable
{
	{ "name",    "ircd.m.media.thumbnail.animation.enable" },
	{ "default", true                                      },
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

decltype(ircd::m::media::thumbnail::mime_whitelist)
ircd::m::media::thumbnail::mime_whitelist
{
	{ "name",     "ircd.m.media.thumbnail.mime.whitelist" },
	{ "default",  "image/jpeg image/png image/webp"       },
};

decltype(ircd::m::media::thumbnail::mime_blacklist)
ircd::m::media::thumbnail::mime_blacklist
{
	{ "name",     "ircd.m.media.thumbnail.mime.blacklist" },
	{ "default",  ""                                      },
};

m::resource
thumbnail_resource
{
	"/_matrix/media/r0/thumbnail/",
	{
		"(11.7.1.4) thumbnails",
		resource::DIRECTORY,
	}
};

static m::resource::response
get__thumbnail_local(client &client,
                     const m::resource::request &request,
                     const m::media::mxc &,
                     const m::room &room);

m::resource::response
get__thumbnail(client &client,
               const m::resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"Server name parameter required"
		};

	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"Media ID parameter required"
		};

	char url_buf[2][256];
	const m::media::mxc mxc
	{
		url::decode(url_buf[0], request.parv[0]),
		url::decode(url_buf[1], request.parv[1]),
	};

	// Thumbnail doesn't require auth so if there is no user_id detected
	// then we download on behalf of @ircd.
	const m::user::id &user_id
	{
		request.user_id?
			m::user::id{request.user_id}:
			m::me()
	};

	if(!m::media::thumbnail::enable_remote)
	{
		const m::room::id::buf room_id
		{
			m::media::file::room_id(mxc)
		};

		if(!exists(room_id))
			return m::resource::response
			{
				client, http::NOT_FOUND
			};
	}

	const m::room::id::buf room_id
	{
		m::media::file::download(mxc, user_id)
	};

	return get__thumbnail_local(client, request, mxc, room_id);
}

static m::resource::method
method_get
{
	thumbnail_resource,
	"GET",
	get__thumbnail,
	{
		m::resource::method::flag(0), // flag
		45s,                          // timeout
	}
};

static m::resource::response
get__thumbnail_local(client &client,
                     const m::resource::request &request,
                     const m::media::mxc &mxc,
                     const m::room &room)
{
	const auto &method
	{
		request.query.get("method", "scale"_sv)
	};

	const size_t _dimension[]
	{
		request.query.get<size_t>("width", 0),
		request.query.get<size_t>("height", 0),
	};

	const pair<size_t> dimension
	{
		_dimension[0]?
			std::clamp(_dimension[0], size_t(width_min), size_t(width_max)):
			_dimension[0],

		_dimension[1]?
			std::clamp(_dimension[1], size_t(height_min), size_t(height_max)):
			_dimension[1]
	};

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
	state.get("ircd.file.stat.size", "", [&file_size]
	(const m::event &event)
	{
		file_size = at<"content"_>(event).get<size_t>("bytes");
	});

	// Get the MIME type
	char type_buf[64];
	string_view content_type
	{
		"application/octet-stream"
	};

	state.get("ircd.file.stat.type", "", [&type_buf, &content_type]
	(const m::event &event)
	{
		const json::string &value
		{
			at<"content"_>(event).at("mime_type")
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
		m::media::file::read(room, sink)
	};

	if(unlikely(read_size != file_size || file_size != copied))
		throw ircd::error
		{
			"File %s/%s [%s] size mismatch: expected %zu got %zu copied %zu",
			mxc.server,
			mxc.mediaid,
			string_view{room.room_id},
			file_size,
			read_size,
			copied
		};

	const auto mime_type
	{
		split(content_type, ';').first
	};

	const bool supported
	{
		// Available in build
		IRCD_USE_MAGICK

		// Enabled by configuration
		&& enable
	};

	const bool permitted
	{
		// If there's a blacklist, mime type must not in the blacklist.
		(!mime_blacklist || !has(mime_blacklist, mime_type))

		// If there's a whitelist, mime type must be in the whitelist.
		&& (!mime_whitelist || has(mime_whitelist, mime_type))
	};

	const bool animated
	{
		// Administrator's fuse to disable animation detection.
		bool(animation_enable)

		// Only call into libpng if magick is supported/enabled
		&& supported

		// If the type is not permitted don't bother checking for animation.
		&& permitted

		// Case for APNG; do not permit them to be thumbnailed
		&& (has(mime_type, "image/png") && png::is_animated(buf))
	};

	const bool valid_args
	{
		// Both dimension parameters given in query string
		(dimension.first && dimension.second)

		// Known thumbnailing method in query string
		&& (method == "scale" || method == "crop")
	};

	const bool fallback // Reasons to just send the original image
	{
		// Thumbnailer support not enabled or available
		!supported

		// Access denied for this operation
		|| !permitted

		// Bypassed to prevent loss of animation
		|| animated

		//  Arguments invalid.
		|| !valid_args
	};

	if(fallback && enable)
		log::dwarning
		{
			"Not thumbnailing %s/%s [%s] '%s' bytes:%zu :%s",
			mxc.server,
			mxc.mediaid,
			string_view{room.room_id},
			content_type,
			file_size,
			!permitted?
				"Not permitted":
			!valid_args?
				"Invalid arguments":
				"Unknown reason",
		};

	static const auto &addl_headers
	{
		"Cache-Control: public, max-age=31536000, immutable\r\n"_sv
	};

	if(fallback)
		return m::resource::response
		{
			client, buf, content_type, http::OK, addl_headers
		};

	const auto closure{[&client, &content_type]
	(const const_buffer &buf)
	{
		m::resource::response
		{
			client, buf, content_type, http::OK, addl_headers
		};
	}};

	if(method == "crop")
		magick::thumbcrop
		{
			buf, dimension, closure
		};
	else
		magick::thumbnail
		{
			buf, dimension, closure
		};

	return {}; // responded from closure.
}
