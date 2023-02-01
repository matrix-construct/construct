// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static resource::response get__dehydrated_device(client &, const m::resource::request &);
	extern resource::method dehydrated_device_get;

	extern resource dehydrated_device_resource;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client :Dehydrated Device"
};

decltype(ircd::m::dehydrated_device_resource)
ircd::m::dehydrated_device_resource
{
	"/_matrix/client/unstable/org.matrix.msc2697.v2/dehydrated_device",
	{
		"msc2697.v2 :Dehydrated Device"
	}
};

decltype(ircd::m::dehydrated_device_get)
ircd::m::dehydrated_device_get
{
	dehydrated_device_resource, "GET", get__dehydrated_device,
	{
		dehydrated_device_get.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::get__dehydrated_device(client &client,
                                const resource::request &request)
{
	const auto device_id
	{
		user::tokens::device(request.access_token)
	};

	const auto algorithm
	{
		"m.dehydration.v1.olm"
	};

	return resource::response
	{
		client, json::members
		{
			{ "device_id", device_id },
			{ "device_data",
			{
				{ "algorithm", algorithm },
			}}
		}
	};
}
