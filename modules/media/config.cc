// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "media.h"

namespace ircd::m::media
{
	extern conf::item<long> m_upload_size;
	static resource::response get_config(client &, const resource::request &);
	extern resource::method config_get;
	extern resource config_resource;
}

decltype(ircd::m::media::config_resource)
ircd::m::media::config_resource
{
	"/_matrix/media/r0/config",
	{
		"r0.6.0-13.8.2.6 conf𝑖g",
	}
};

decltype(ircd::m::media::config_get)
ircd::m::media::config_get
{
	config_resource, "GET", get_config
};

decltype(ircd::m::media::m_upload_size)
ircd::m::media::m_upload_size
{
	{ "name",      "ircd.m.media.m.upload.size" },
	{ "default",   long(64_MiB)                 },
};

ircd::m::resource::response
ircd::m::media::get_config(client &client,
                           const resource::request &request)
{
	return resource::response
	{
		client, json::members
		{
			{ "m.upload.size", long(m_upload_size) }
		}
	};
}
