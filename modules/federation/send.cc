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

log::log
txn_log
{
	"m.txn"
};

m::resource
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
	{ "default",  false                             },
};

conf::item<size_t>
eval_max_per_node
{
	{ "name",     "ircd.federation.send.eval.max_per_node" },
	{ "default",  4L                                       },
};

conf::item<bool>
fetch_state
{
	{ "name",     "ircd.federation.send.fetch_state" },
	{ "default",  true                               },
};

conf::item<bool>
fetch_prev
{
	{ "name",     "ircd.federation.send.fetch_prev" },
	{ "default",  true                              },
};

static void
handle_edu(client &client,
           const m::resource::request::object<m::txn> &request,
           const string_view &txn_id,
           const m::edu &edu)
{
	m::event event;
	json::get<"origin"_>(event) = request.node_id;
	json::get<"origin_server_ts"_>(event) = at<"origin_server_ts"_>(request);
	json::get<"content"_>(event) = at<"content"_>(edu);
	json::get<"type"_>(event) = at<"edu_type"_>(edu);
	json::get<"depth"_>(event) = json::undefined_number;

	m::vm::opts vmopts;
	vmopts.nothrows = -1U;
	vmopts.node_id = request.node_id;
	vmopts.txn_id = txn_id;
	vmopts.edu = true;
	vmopts.notify_clients = false;
	vmopts.notify_servers = false;
	m::vm::eval eval
	{
		event, vmopts
	};
}

static void
handle_pdus(client &client,
            const m::resource::request::object<m::txn> &request,
            const string_view &txn_id,
            const json::array &pdus,
            json::stack &out)
{
	json::stack::object out_pdus
	{
		out, "pdus"
	};

	m::vm::opts vmopts;
	vmopts.out = &out_pdus;
	vmopts.warnlog = 0;
	vmopts.infolog_accept = true;
	vmopts.nothrows = -1U;
	vmopts.node_id = request.node_id;
	vmopts.txn_id = txn_id;
	vmopts.phase.set(m::vm::phase::FETCH_PREV, bool(fetch_prev));
	vmopts.phase.set(m::vm::phase::FETCH_STATE, bool(fetch_state));
	vmopts.fetch_prev_wait_count = -1;
	m::vm::eval eval
	{
		pdus, vmopts
	};
}

static void
handle_txn(client &client,
           const m::resource::request::object<m::txn> &request,
           const string_view &txn_id,
           json::stack &out)
try
{
	// We process PDU's before EDU's and we process all PDU's at once by
	// passing the complete array. The events are sorted and dependencies
	// are detected within the array. If we looped here for eval'ing one
	// at a time we'd risk issuing fetch requests for prev_events which may
	// exist in the same array, etc.
	if(!empty(json::get<"pdus"_>(request)))
		handle_pdus(client, request, txn_id, json::get<"pdus"_>(request), out);

	// We process EDU's after PDU's. This is because checks on EDU's may
	// depend on updates provided by PDU's in the same txn; for example:
	// 1. user X joins room Y. 2. user X starts typing in room Y. Note that
	// we also process EDU's one at a time since there is no dependency graph
	// or anything like that so if this loop wasn't here it would just be
	// somewhere else.
	for(const json::object edu : json::get<"edus"_>(request))
		handle_edu(client, request, txn_id, edu);
}
catch(const m::vm::error &e)
{
	const json::object &content
	{
		e.content
	};

	const json::string error[]
	{
		content["errcode"],
		content["error"]
	};

	log::error
	{
		txn_log, "Unhandled error processing txn '%s' from '%s' :%s :%s :%s",
		txn_id,
		request.node_id,
		e.what(),
		error[0],
		error[1],
	};

	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		txn_log, "Unhandled error processing txn '%s' from '%s' :%s",
		txn_id,
		request.node_id,
		e.what(),
	};

	throw;
}

m::resource::response
handle_put(client &client,
           const m::resource::request::object<m::txn> &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"txn_id path parameter required"
		};

	char txn_id_buf[128];
	const auto txn_id
	{
		url::decode(txn_id_buf, request.parv[0])
	};

	const string_view &origin
	{
		json::at<"origin"_>(request)
	};

	if(origin && origin != request.node_id)
		throw m::ACCESS_DENIED
		{
			"txn[%s] originating from '%s' not accepted when relayed by '%s'",
			txn_id,
			origin,
			request.node_id,
		};

	// Don't accept sends to ourself for whatever reason (i.e a 127.0.0.1
	// leaked into the target list). This should be a 500 so it's not
	// considered success or cached as failure by the sender's state.
	if(unlikely(my_host(request.node_id)) && !bool(allow_self))
		throw m::error
		{
			"M_SEND_TO_SELF", "Tried to send %s from myself to myself.",
			txn_id
		};

	size_t evals{0};
	bool txn_in_progress{false};
	m::vm::eval::for_each([&txn_id, &request, &evals, &txn_in_progress]
	(const auto &eval) noexcept
	{
		assert(eval.opts);
		const bool match_node
		{
			eval.opts->node_id == request.node_id
		};

		const bool match_txn
		{
			match_node &&
			eval.opts->txn_id == txn_id
		};

		evals += match_node;
		txn_in_progress |= match_txn;
		return evals < size_t(eval_max_per_node);
	});

	if(evals >= size_t(eval_max_per_node))
		return m::resource::response
		{
			client, http::TOO_MANY_REQUESTS
		};

	if(txn_in_progress)
		return m::resource::response
		{
			client, http::ACCEPTED
		};

	char rembuf[96];
	log::logf
	{
		txn_log, log::level::DEBUG,
		"%s %zu$B pdu:%zu %zu$B edu:%zu %zu %s :%s",
		txn_id,
		size(string_view(json::get<"pdus"_>(request))),
		json::get<"pdus"_>(request).count(),
		size(string_view(json::get<"edus"_>(request))),
		json::get<"edus"_>(request).count(),
		evals,
		string(rembuf, remote(client)),
		origin,
	};

	char chunk[1536];
	m::resource::response::chunked response
	{
		client, http::OK, 0UL, chunk
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::object top
	{
		out
	};

	handle_txn(client, request, txn_id, out);
	return response;
}

m::resource::method
method_put
{
	send_resource, "PUT", handle_put,
	{
		method_put.VERIFY_ORIGIN,

		// Coarse timeout
		90s, //TODO: conf

		// Payload maximum
		4_MiB // larger = HTTP 413  //TODO: conf
	}
};
