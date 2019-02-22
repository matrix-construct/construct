// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Client 14.9 :Send-to-Device messaging"
};

ircd::resource
send_to_device_resource
{
	"/_matrix/client/r0/sendToDevice/",
	{
		"(14.9.3) Protocol definitions",
		resource::DIRECTORY,
	}
};

ircd::resource::redirect::permanent
send_to_device_resource__unstable
{
	"/_matrix/client/unstable/sendToDevice/",
	"/_matrix/client/r0/sendToDevice/",
	{
		"(14.9.3) Protocol definitions",
		resource::DIRECTORY,
	}
};

static resource::response
put__send_to_device(client &client,
                    const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"event type path parameter required"
		};

	char typebuf[m::event::TYPE_MAX_SIZE];
	const string_view &type
	{
		url::decode(typebuf, request.parv[0])
	};

	if(request.parv.size() < 2)
		throw m::NEED_MORE_PARAMS
		{
			"txnid path parameter required"
		};

	char txnidbuf[256];
	const string_view &txnid
	{
		url::decode(txnidbuf, request.parv[1])
	};

	const json::object &messages
	{
		request["messages"]
	};

	return resource::response
	{
		client, http::OK
	};
}

resource::method
method_put
{
	send_to_device_resource, "PUT", put__send_to_device,
	{
		method_put.REQUIRES_AUTH
	}
};
