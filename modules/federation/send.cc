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
	{ "default",  1L                                       },
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

void
handle_edu(client &client,
           const m::resource::request::object<m::txn> &request,
           const string_view &txn_id,
           const m::edu &edu)
try
{
	m::event event;
	json::get<"origin"_>(event) = request.origin;
	json::get<"origin_server_ts"_>(event) = at<"origin_server_ts"_>(request);
	json::get<"content"_>(event) = at<"content"_>(edu);
	json::get<"type"_>(event) = at<"edu_type"_>(edu);
	json::get<"depth"_>(event) = json::undefined_number;

	m::vm::opts vmopts;
	vmopts.node_id = request.origin;
	vmopts.txn_id = txn_id;
	vmopts.edu = true;
	vmopts.notify_clients = false;
	vmopts.notify_servers = false;
	m::vm::eval eval
	{
		event, vmopts
	};
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::derror
	{
		m::log, "%s :%s EDU :%s",
		txn_id,
		request.origin,
		e.what(),
	};
}

void
handle_pdus(client &client,
            const m::resource::request::object<m::txn> &request,
            const string_view &txn_id,
            const json::array &pdus)
{
	m::vm::opts vmopts;
	vmopts.warnlog = 0;
	vmopts.infolog_accept = true;
	vmopts.nothrows = -1U;
	vmopts.nothrows &= ~m::vm::fault::INTERRUPT;
	vmopts.node_id = request.origin;
	vmopts.txn_id = txn_id;
	vmopts.fetch_prev = bool(fetch_state);
	vmopts.fetch_state = bool(fetch_prev);
	m::vm::eval eval
	{
		pdus, vmopts
	};
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

	const json::array &edus
	{
		json::get<"edus"_>(request)
	};

	const json::array &pdus
	{
		json::get<"pdus"_>(request)
	};

	log::debug
	{
		m::log, "%s :%s | %s --> edus:%zu pdus:%zu",
		txn_id,
		origin,
		string(remote(client)),
		edus.count(),
		pdus.count(),
	};

	if(origin && origin != request.origin)
		throw m::ACCESS_DENIED
		{
			"txn[%s] originating from '%s' not accepted when relayed by '%s'",
			txn_id,
			origin,
			request.origin,
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

	size_t evals{0};
	const bool txn_in_progress
	{
		!m::vm::eval::for_each([&txn_id, &request, &evals]
		(const auto &eval)
		{
			assert(eval.opts);

			const bool match_node
			{
				eval.opts->node_id == request.origin
			};

			const bool match_txn
			{
				match_node &&
				eval.opts->txn_id == txn_id
			};

			evals += match_node;
			return !match_txn; // false to break; for_each() returns false
		})
	};

	if(txn_in_progress)
		return m::resource::response
		{
			client, http::ACCEPTED
		};

	if(evals >= size_t(eval_max_per_node))
		return m::resource::response
		{
			client, http::TOO_MANY_REQUESTS
		};

	handle_pdus(client, request, txn_id, pdus);

	for(const json::object &edu : edus)
		handle_edu(client, request, txn_id, edu);

	return m::resource::response
	{
		client, http::OK
	};
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
