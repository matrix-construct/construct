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
download_resource
{
	"/_matrix/media/r0/download/",
	{
		"(11.7.1.2) download",
		resource::DIRECTORY,
	}
};

static m::resource::response
get__download_local(client &client,
                    const m::resource::request &request,
                    const m::media::mxc &mxc,
                    const m::room &room);

static m::resource::response
get__download(client &client,
              const m::resource::request &request)
{
	if(request.parv.size() < 2)
		throw http::error
		{
			http::MULTIPLE_CHOICES, "/ download / domain / file"
		};

	char url_buf[2][256];
	const m::media::mxc mxc
	{
		url::decode(url_buf[0], request.parv[0]),
		url::decode(url_buf[1], request.parv[1]),
	};

	// Download doesn't require auth so if there is no user_id detected
	// then we download on behalf of @ircd.
	const m::user::id &user_id
	{
		request.user_id?
			m::user::id{request.user_id}:
			m::me()
	};

	const bool allow_remote
	{
		request.query.get<bool>("allow_remote", true)
	};

	const m::room::id::buf room_id
	{
		m::media::file::download(mxc, user_id)
	};

	return get__download_local(client, request, mxc, room_id);
}

static m::resource::response
get__download_local(client &client,
                    const m::resource::request &request,
                    const m::media::mxc &mxc,
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

	static const auto &addl_headers
	{
		"Cache-Control: public, max-age=31536000, immutable\r\n"_sv
	};

	// Send HTTP head to client
	m::resource::response
	{
		client,
		http::OK,
		content_type,
		file_size,
		addl_headers,
	};

	size_t sent{0}, read
	{
		m::media::file::read(room, [&client, &sent]
		(const string_view &block)
		{
			sent += write_all(*client.sock, block);
		})
	};

	if(unlikely(read != file_size))
		log::error
		{
			m::media::log, "File %s/%s [%s] size mismatch: expected %zu got %zu",
			mxc.server,
			mxc.mediaid,
			string_view{room.room_id},
			file_size,
			read
		};

	// Have to kill client here after failing content length expectation.
	if(unlikely(read != file_size))
		client.close(net::dc::RST, net::close_ignore);

	return {};
}

static m::resource::method
method_get
{
	download_resource,
	"GET",
	get__download,
	{
		m::resource::method::flag(0),  // flags
		45s,                           // timeout
	}
};
