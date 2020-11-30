// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::relation
{
	static void handle_fetch(const event &, vm::eval &);
	extern hookfn<vm::eval &> fetch_hook;
	extern conf::item<seconds> fetch_timeout;
	extern conf::item<bool> fetch_enable;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix relations"
};

decltype(ircd::m::relation::fetch_enable)
ircd::m::relation::fetch_enable
{
	{ "name",     "ircd.m.relation.fetch.enable" },
	{ "default",  true                           },
};

decltype(ircd::m::relation::fetch_timeout)
ircd::m::relation::fetch_timeout
{
	{ "name",     "ircd.m.relation.fetch.timeout" },
	{ "default",  5L                              },
};

decltype(ircd::m::relation::fetch_hook)
ircd::m::relation::fetch_hook
{
	handle_fetch,
	{
		{ "_site",  "vm.fetch.prev" },
	}
};

void
ircd::m::relation::handle_fetch(const event &event,
                                vm::eval &eval)
try
{
	assert(eval.opts);
	const auto &opts{*eval.opts};
	if(!opts.fetch || !fetch_enable)
		return;

	if(my(event))
		return;

	// event must be in a room for now; we won't have context until dht
	if(!json::get<"room_id"_>(event))
		return;

	const auto &content
	{
		json::get<"content"_>(event)
	};

	const json::object &m_relates_to
	{
		content["m.relates_to"]
	};

	if(!m_relates_to || !json::type(m_relates_to, json::OBJECT))
		return;

	const json::object &m_in_reply_to
	{
		m_relates_to["m.in_reply_to"]
	};

	const m::event::id &event_id
	{
		json::type(m_in_reply_to, json::OBJECT)?
			m_in_reply_to.get<json::string>("event_id"):
			m_relates_to.get<json::string>("event_id")
	};

	// If the relates_to is a prev_event then the vm::fetch unit will perform
	// the fetch so this will just be redundant and we can bail.
	const event::prev prev{event};
	if(prev.prev_events_has(event_id))
		return;

	if(likely(m::exists(event_id)))
		return;

	log::dwarning
	{
		log, "%s in %s by %s relates to missing %s; fetching...",
		string_view(event.event_id),
		string_view(at<"room_id"_>(event)),
		string_view(at<"sender"_>(event)),
		string_view(event_id),
	};

	m::fetch::opts fetch_opts;
	fetch_opts.op = m::fetch::op::event;
	fetch_opts.room_id = at<"room_id"_>(event);
	fetch_opts.event_id = event_id;
	auto request
	{
		m::fetch::start(fetch_opts)
	};

	const auto response
	{
		request.get(seconds(fetch_timeout))
	};

	const json::array pdus
	{
		json::object(response).get("pdus")
	};

	if(pdus.empty())
		return;

	const m::event result
	{
		json::object(pdus.at(0)), event_id
	};

	auto eval_opts(opts);
	eval_opts.phase.set(vm::phase::FETCH_PREV, false);
	eval_opts.phase.set(vm::phase::FETCH_STATE, false);
	eval_opts.node_id = response.origin;
	vm::eval
	{
		result, eval_opts
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
		log, "Failed to fetch relation for %s in %s :%s",
		string_view(event.event_id),
		string_view(json::get<"room_id"_>(event)),
		e.what(),
	};
}
