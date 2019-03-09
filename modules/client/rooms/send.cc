// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "rooms.h"

using namespace ircd::m;
using namespace ircd;

resource::response
put__send(client &client,
          const resource::request &request,
          const room::id &room_id)
{
	if(request.parv.size() < 3)
		throw NEED_MORE_PARAMS
		{
			"type parameter missing"
		};

	char type_buf[m::event::TYPE_MAX_SIZE];
	const string_view &type
	{
		url::decode(type_buf, request.parv[2])
	};

	if(request.parv.size() < 4)
		throw NEED_MORE_PARAMS
		{
			"txnid parameter missing"
		};

	char transaction_id_buf[64];
	const string_view &transaction_id
	{
		url::decode(transaction_id_buf, request.parv[3])
	};

	m::vm::copts copts;
	copts.client_txnid = transaction_id;

	room room
	{
		room_id, &copts
	};

	const json::object &content
	{
		request.content
	};

	const auto event_id
	{
		send(room, request.user_id, type, content)
	};

	return resource::response
	{
		client, json::members
		{
			{ "event_id", event_id }
		}
	};
}

static void
save_transaction_id(const m::event &event,
                    m::vm::eval &eval);

m::hookfn<m::vm::eval &>
save_transaction_id_hookfn
{
	save_transaction_id,
	{
		{ "_site",    "vm.post" },
		{ "origin",   my_host() },
	}
};

void
save_transaction_id(const m::event &event,
                    m::vm::eval &eval)
{
	if(!eval.copts)
		return;

	if(!eval.copts->client_txnid)
		return;

	if(!json::get<"event_id"_>(event))
		return;

	assert(my_host(at<"origin"_>(event)));
	const m::user::room user_room
	{
		at<"sender"_>(event)
	};

	static const string_view &type{"ircd.client.txnid"};
	const auto &state_key
	{
		at<"event_id"_>(event)
	};

	send(user_room, at<"sender"_>(event), type, state_key,
	{
		{ "transaction_id", eval.copts->client_txnid }
	});
}
