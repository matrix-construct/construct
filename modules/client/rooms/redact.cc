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

m::resource::response
put__redact(client &client,
            const m::resource::request &request,
            const room::id &room_id)
{
	if(request.parv.size() < 3)
		throw NEED_MORE_PARAMS
		{
			"event_id parameter missing"
		};

	event::id::buf redacts
	{
		url::decode(redacts, request.parv[2])
	};

	if(request.parv.size() < 4)
		throw NEED_MORE_PARAMS
		{
			"txnid parameter missing"
		};

	char txnid_buf[64];
	const string_view &txnid
	{
		url::decode(txnid_buf, request.parv[3])
	};

	const json::string &reason
	{
		request["reason"]
	};

	m::vm::copts vmopts;
	vmopts.client_txnid = txnid;
	const room room
	{
		room_id, &vmopts
	};

	const auto event_id
	{
		m::redact(room, request.user_id, redacts, reason)
	};

	return m::resource::response
	{
		client, json::members
		{
			{ "event_id", event_id }
		}
	};
}

m::resource::response
post__redact(client &client,
             const m::resource::request &request,
             const room::id &room_id)
{
	if(request.parv.size() < 3)
		throw NEED_MORE_PARAMS
		{
			"event_id parameter missing"
		};

	event::id::buf redacts
	{
		url::decode(redacts, request.parv[2])
	};

	const room room
	{
		room_id
	};

	const auto &reason
	{
		unquote(request["reason"])
	};

	const auto event_id
	{
		m::redact(room, request.user_id, redacts, reason)
	};

	return m::resource::response
	{
		client, json::members
		{
			{ "event_id", event_id }
		}
	};
}
