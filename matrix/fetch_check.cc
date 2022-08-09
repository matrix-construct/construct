// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::fetch
{
	static void check_event_signature(const request &, const m::event &);
	static void check_event_conforms(const request &, const m::event &);
	static void check_event_id(const request &, const m::event &);
	static void check_event(const request &, const m::event &);
	static void check_response__backfill(const request &, const json::object &);
	static void check_response__event(const request &, const json::object &);
	static void check_response__auth(const request &, const json::object &);
	extern void check_response(const request &, const json::object &);

	extern conf::item<bool> enable_check_event_id;
	extern conf::item<bool> enable_check_conforms;
	extern conf::item<bool> enable_check_signature;
	extern conf::item<bool> enable_check_hashes;
	extern conf::item<bool> enable_check_authoritative_redaction;
}

decltype(ircd::m::fetch::enable_check_event_id)
ircd::m::fetch::enable_check_event_id
{
	{ "name",     "ircd.m.fetch.check.event_id" },
	{ "default",  true                          },
};

decltype(ircd::m::fetch::enable_check_conforms)
ircd::m::fetch::enable_check_conforms
{
	{ "name",     "ircd.m.fetch.check.conforms" },
	{ "default",  true                          },
};

decltype(ircd::m::fetch::enable_check_hashes)
ircd::m::fetch::enable_check_hashes
{
	{ "name",     "ircd.m.fetch.check.hashes" },
	{ "default",  true                        },
};

decltype(ircd::m::fetch::enable_check_authoritative_redaction)
ircd::m::fetch::enable_check_authoritative_redaction
{
	{ "name",     "ircd.m.fetch.check.authoritative_redaction" },
	{ "default",  true                                         },
};

decltype(ircd::m::fetch::enable_check_signature)
ircd::m::fetch::enable_check_signature
{
	{ "name",         "ircd.m.fetch.check.signature" },
	{ "default",      true                           },
	{ "description",

	R"(
	false - Signatures of events will not be checked by the fetch unit (they
	are still checked normally during evaluation; this conf item does not
	disable event signature verification for the server).

	true - Signatures of events will be checked by the fetch unit such that
	bogus responses allow the fetcher to try the next server. This check might
	not occur in all cases. It will only occur if the server has the public
	key already; fetch unit worker contexts cannot be blocked trying to obtain
	unknown keys from remote hosts.
	)"},
};

void
ircd::m::fetch::check_response(const request &request,
                               const json::object &response)
{
	switch(request.opts.op)
	{
		case op::backfill:
			return check_response__backfill(request, response);

		case op::event:
			return check_response__event(request, response);

		case op::auth:
			return check_response__auth(request, response);

		case op::noop:
			return;
	}

	ircd::panic
	{
		"Unchecked response; fetch op:%u",
		uint(request.opts.op),
	};
}

void
ircd::m::fetch::check_response__auth(const request &request,
                                     const json::object &response)
{
	const json::array &auth_chain
	{
		response.at("auth_chain")
	};

	for(const json::object auth_event : auth_chain)
	{
		m::event::id::buf event_id;
		const m::event event
		{
			event_id, auth_event
		};

		check_event(request, event);
	}
}

void
ircd::m::fetch::check_response__event(const request &request,
                                      const json::object &response)
{
	const json::array &pdus
	{
		response.at("pdus")
	};

	const m::event event
	{
		pdus.at(0), request.opts.event_id
	};

	check_event(request, event);
}

void
ircd::m::fetch::check_response__backfill(const request &request,
                                         const json::object &response)
{
	const json::array &pdus
	{
		response.at("pdus")
	};

	for(const json::object event : pdus)
	{
		m::event::id::buf event_id;
		const m::event _event
		{
			event_id, event
		};

		check_event(request, _event);
	}
}

void
ircd::m::fetch::check_event(const request &request,
                            const m::event &event)
{
	if(unlikely(!request.promise))
		throw ctx::broken_promise
		{
			"Fetch response check interrupted."
		};

	if(request.opts.check_event_id && enable_check_event_id)
		check_event_id(request, event);

	if(request.opts.check_conforms && enable_check_conforms)
		check_event_conforms(request, event);

	// only check signature for v1 events
	assert(request.promise);
	if(request.opts.check_signature && enable_check_signature && request.opts.event_id.version() == "1")
		check_event_signature(request, event);
}

void
ircd::m::fetch::check_event_id(const request &request,
                               const m::event &event)
{
	if(likely(m::check_id(event)))
		return;

	event::id::buf buf;
	const m::event &claim
	{
		buf, event.source
	};

	throw ircd::error
	{
		"event::id claim:%s != sought:%s",
		string_view{claim.event_id},
		string_view{request.opts.event_id},
	};
}

void
ircd::m::fetch::check_event_conforms(const request &request,
                                     const m::event &event)
{
	m::event::conforms conforms
	{
		event
	};

	const bool mismatch_hashes
	{
		enable_check_hashes
		&& request.opts.check_hashes
		&& conforms.has(m::event::conforms::MISMATCH_HASHES)
	};

	const bool authoritative_redaction
	{
		enable_check_authoritative_redaction
		&& request.opts.authoritative_redaction
		&& mismatch_hashes
		&& json::get<"origin"_>(event) == request.origin
	};

	if(mismatch_hashes && !authoritative_redaction)
	{
		const json::object _unsigned
		{
			event.source["unsigned"]
		};

		const json::string redacted_by
		{
			_unsigned["redacted_by"]
		};

		if(valid(id::EVENT, redacted_by))
			log::dwarning
			{
				log, "%s claims %s redacted by %s",
				request.origin,
				string_view{request.opts.event_id},
				redacted_by,
			};

		//TODO: XXX
	}

	if(authoritative_redaction || !mismatch_hashes)
		conforms.del(m::event::conforms::MISMATCH_HASHES);

	thread_local char buf[128];
	const string_view failures
	{
		conforms.string(buf)
	};

	assert(failures || conforms.clean());
	if(!conforms.clean())
		throw ircd::error
		{
			"Non-conforming event in response :%s",
			failures,
		};
}

void
ircd::m::fetch::check_event_signature(const request &request,
                                      const m::event &event)
{
	const string_view &server
	{
		!json::get<"origin"_>(event)?
			user::id(at<"sender"_>(event)).host():
			string_view(json::get<"origin"_>(event))
	};

	const json::object &signatures
	{
		at<"signatures"_>(event).at(server)
	};

	const json::string &key_id
	{
		!signatures.empty()?
			signatures.begin()->first:
			string_view{},
	};

	if(!key_id)
		throw ircd::error
		{
			"Cannot find any keys for '%s' in event.signatures",
			server,
		};

	if(m::keys::cache::has(server, key_id))
		if(!verify(event, server))
			throw ircd::error
			{
				"Signature verification failed."
			};
}
