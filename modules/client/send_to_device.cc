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

static void
send_to_device(const string_view &txnid,
               const m::user::id &sender,
               const m::user::id &target,
               const string_view &type,
               const json::object &message);

static resource::response
put__send_to_device(client &client,
                    const resource::request &request);

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

ircd::resource
send_to_device_resource__unstable
{
	"/_matrix/client/unstable/sendToDevice/",
	{
		"(14.9.3) Protocol definitions",
		resource::DIRECTORY,
	}
};

resource::method
method_put
{
	send_to_device_resource, "PUT", put__send_to_device,
	{
		method_put.REQUIRES_AUTH
	}
};

resource::method
method_put__unstable
{
	send_to_device_resource__unstable, "PUT", put__send_to_device,
	{
		method_put.REQUIRES_AUTH
	}
};

resource::response
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

	if(size(messages) > 1)
		throw m::UNSUPPORTED
		{
			"Multiple user targets is not yet supported."
		};

	for(const auto &[user_id, message] : messages)
		send_to_device(txnid, request.user_id, user_id, type, messages);

	return resource::response
	{
		client, http::OK
	};
}

void
send_to_device(const string_view &txnid,
               const m::user::id &sender,
               const m::user::id &target,
               const string_view &type,
               const json::object &message)
{
	json::iov event, content;
	const json::iov::push push[]
	{
		// The federation sender considers the room_id property of an event
		// as the "destination" and knows what to do if it's actually some
		// some other string like the user::id we're targeting here.
		{ event,   { "room_id",      target              } },
		{ event,   { "type",        "m.direct_to_device" } },
		{ content, { "type",         type                } },
		{ content, { "sender",       sender              } },
		{ content, { "message_id",   txnid               } },
		{ content, { "messages",     message             } },
	};

	m::vm::copts opts;
	opts.add_hash = false;
	opts.add_sig = false;
	opts.add_event_id = false;
	opts.add_origin = true;
	opts.add_origin_server_ts = false;
	opts.conforming = false;
	opts.notify_clients = false;
	m::vm::eval
	{
		event, content, opts
	};
}
