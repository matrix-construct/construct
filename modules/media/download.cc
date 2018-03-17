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
	"Client 11.7 :Content repository (download)"
};

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

resource::response
get__download(client &client,
              const resource::request &request)
{
	if(request.parv.size() < 2)
		throw http::error
		{
			http::MULTIPLE_CHOICES, "/ download / domain / file"
		};

	const auto &domain
	{
		request.parv[0]
	};

	const auto &file
	{
		request.parv[1]
	};

	const std::string data
	{
		//fs::read(file)
	};

	char mime_type_buf[64];
	const string_view content_type
	{
		magic::mime(mime_type_buf, string_view{data})
	};

	return resource::response
	{
		client, string_view{data}, content_type
	};
}

resource::method
method_get
{
	download_resource, "GET", get__download
};

resource::method
method_get__legacy
{
	download_resource__legacy, "GET", get__download
};
