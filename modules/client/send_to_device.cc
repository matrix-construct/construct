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
               const string_view &device_id,
               const string_view &type,
               const json::object &message);

static m::resource::response
put__send_to_device(client &client,
                    const m::resource::request &request);

mapi::header
IRCD_MODULE
{
	"Client 14.9 :Send-to-Device messaging"
};

ircd::m::resource
send_to_device_resource
{
	"/_matrix/client/r0/sendToDevice/",
	{
		"(14.9.3) Protocol definitions",
		resource::DIRECTORY,
	}
};

m::resource::method
method_put
{
	send_to_device_resource, "PUT", put__send_to_device,
	{
		method_put.REQUIRES_AUTH
	}
};

m::resource::response
put__send_to_device(client &client,
                    const m::resource::request &request)
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

	const json::object &targets
	{
		request["messages"]
	};

	for(const auto &[user_id, messages] : targets)
		for(const auto &[device_id, message] : json::object(messages))
			send_to_device(txnid, request.user_id, user_id, device_id, type, message);

	return m::resource::response
	{
		client, http::OK
	};
}

void
send_to_device(const string_view &txnid,
               const m::user::id &sender,
               const m::user::id &target,
               const string_view &device_id,
               const string_view &type,
               const json::object &message)
try
{
	const unique_buffer<mutable_buffer> buf
	{
		48_KiB
	};

	json::stack out{buf};
	{
		json::stack::object _messages
		{
			out
		};

		json::stack::object _target
		{
			_messages, target
		};

		json::stack::object _device
		{
			_target, device_id
		};

		_device.append(message);
	}

	json::iov event, content;
	const json::iov::push push[]
	{
		{ event,   { "type",        "m.direct_to_device"     } },
		{ content, { "type",        type                     } },
		{ content, { "sender",      sender                   } },
		{ content, { "target",      target                   } },
		{ content, { "message_id",  { txnid, json::STRING }  } },
		{ content, { "messages",    out.completed()          } },
	};

	m::vm::copts opts;
	opts.prop_mask.reset();
	opts.prop_mask.set("origin");
	opts.edu = true;
	opts.notify_clients = false;
	m::vm::eval
	{
		event, content, opts
	};
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "Send %s '%s' by %s to device '%s' belonging to %s :%s",
		type,
		txnid,
		string_view{sender},
		device_id,
		string_view{target},
		e.what(),
	};
}
