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
	"federation send"
};

resource
send_resource
{
	"/_matrix/federation/v1/send/",
	{
		"federation send",
		resource::DIRECTORY,
	}
};

conf::item<bool>
allow_self
{
	{ "name",     "ircd.federation.send.allow_self" },
	{ "default",  "false"                           },
};

void
handle_edu(client &client,
           const resource::request::object<m::txn> &request,
           const string_view &txn_id,
           const m::edu &edu)
{
	m::event event;
	json::get<"origin"_>(event) = request.origin;
	json::get<"origin_server_ts"_>(event) = at<"origin_server_ts"_>(request);
	json::get<"content"_>(event) = at<"content"_>(edu);
	json::get<"type"_>(event) = at<"edu_type"_>(edu);
	json::get<"depth"_>(event) = json::undefined_number;

	m::vm::opts vmopts;
	vmopts.non_conform.set(m::event::conforms::INVALID_OR_MISSING_EVENT_ID);
	vmopts.non_conform.set(m::event::conforms::INVALID_OR_MISSING_ROOM_ID);
	vmopts.non_conform.set(m::event::conforms::INVALID_OR_MISSING_SENDER_ID);
	vmopts.non_conform.set(m::event::conforms::MISSING_ORIGIN_SIGNATURE);
	vmopts.non_conform.set(m::event::conforms::MISSING_SIGNATURES);
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_EVENTS);
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	vmopts.non_conform.set(m::event::conforms::DEPTH_ZERO);
	m::vm::eval eval
	{
		event, vmopts
	};
}

void
handle_pdus(client &client,
            const resource::request::object<m::txn> &request,
            const string_view &txn_id,
            const json::array &pdus)
{
	m::vm::opts vmopts;
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	vmopts.prev_check_exists = false;
	vmopts.nothrows = -1U;
	vmopts.infolog_accept = true;
	vmopts.warnlog |= m::vm::fault::STATE;
	vmopts.errorlog &= ~m::vm::fault::STATE;
	m::vm::eval eval
	{
		pdus, vmopts
	};
}

void
handle_pdu_failure(client &client,
                   const resource::request::object<m::txn> &request,
                   const string_view &txn_id,
                   const json::object &pdu_failure)
{
	log::error
	{
		m::log, "%s :%s | (pdu_failure) %s",
		txn_id,
		at<"origin"_>(request),
		pdu_failure.get("sender", string_view{"*"}),
		string_view{pdu_failure}
	};
}

resource::response
handle_put(client &client,
           const resource::request::object<m::txn> &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"txn_id path parameter required"
		};

	const auto txn_id
	{
		request.parv[0]
	};

	const string_view &origin
	{
		json::at<"origin"_>(request)
	};

	const json::array &edus
	{
		json::get<"edus"_>(request)
	};

	const json::array &pdus
	{
		json::get<"pdus"_>(request)
	};

	const json::array &pdu_failures
	{
		json::get<"pdu_failures"_>(request)
	};

	log::debug
	{
		m::log, "%s :%s | %s --> edus:%zu pdus:%zu errors:%zu",
		txn_id,
		origin,
		string(remote(client)),
		edus.count(),
		pdus.count(),
		pdu_failures.count()
	};

	// Don't accept sends to ourself for whatever reason (i.e a 127.0.0.1
	// leaked into the target list). This should be a 500 so it's not
	// considered success or cached as failure by the sender's state.
	if(unlikely(my_host(request.origin)) && !bool(allow_self))
		throw m::error
		{
			"M_SEND_TO_SELF", "Tried to send %s from myself to myself.",
			txn_id
		};

	for(const auto &pdu_failure : pdu_failures)
		handle_pdu_failure(client, request, txn_id, pdu_failure);

	handle_pdus(client, request, txn_id, pdus);

	for(const json::object &edu : edus)
		handle_edu(client, request, txn_id, edu);

	return resource::response
	{
		client, http::OK
	};
}

resource::method
method_put
{
	send_resource, "PUT", handle_put,
	{
		method_put.VERIFY_ORIGIN,

		// Coarse timeout
		45s,

		// Payload maximum
		4_MiB // larger = HTTP 413  //TODO: conf
	}
};
