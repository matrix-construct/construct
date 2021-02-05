// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::bridge
{
	static size_t make_txn(const config &, json::stack &, events::range &);
	static event::idx worker_handle(const config &, const net::hostport &, const events::range &, window_buffer);
	static void worker_loop(const config &, const rfc3986::uri &, const mutable_buffer &);
	static void worker(std::string event, std::string event_id);
	static void handle_event(const event &, vm::eval &);
	static void fini();
	static void init();

	extern conf::item<bool> enable;
	extern conf::item<seconds> timeout;
	extern ctx::dock worker_dock;
	extern std::vector<context> worker_context;
	extern hookfn<vm::eval &> notify_hook;
}

ircd::mapi::header
IRCD_MODULE
{
	"Bridges (Application Services)",
	ircd::m::bridge::init,
	ircd::m::bridge::fini,
};

decltype(ircd::m::bridge::enable)
ircd::m::bridge::enable
{
	{ "name",      "ircd.m.bridge.enable"  },
	{ "default",   true                    },
};

decltype(ircd::m::bridge::timeout)
ircd::m::bridge::timeout
{
	{ "name",      "ircd.m.bridge.txn.timeout"  },
	{ "default",   10L                          },
};

decltype(ircd::m::bridge::worker_dock)
ircd::m::bridge::worker_dock;

decltype(ircd::m::bridge::worker_context)
ircd::m::bridge::worker_context;

decltype(ircd::m::bridge::notify_hook)
ircd::m::bridge::notify_hook
{
	handle_event,
	{
		{ "_site", "vm.notify" },
	}
};

void
ircd::m::bridge::handle_event(const m::event &event,
                              vm::eval &eval)
try
{
	if(eval.room_internal)
		return;

	if(!event.event_id)
		return;

	worker_dock.notify_all();
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Failed to handle %s notify :%s",
		string_view{event.event_id},
		e.what(),
	};
}

void
ircd::m::bridge::init()
{
	if(!enable)
		return;

	config::for_each([]
	(const event::idx &event_idx, const event &event, const config &config)
	{
		log::debug
		{
			log, "Found configuration for '%s' in %s by %s",
			json::get<"id"_>(config),
			json::get<"room_id"_>(event),
			string_view{event.event_id},
		};

		worker_context.emplace_back(context
		{
			"m.bridge",
			512_KiB,
			context::POST,
			std::bind(&bridge::worker, std::string(event.source), std::string(event.event_id)),
		});

		return true;
	});
}

void
ircd::m::bridge::fini()
{
	for(auto &worker : worker_context)
		worker.terminate();

	if(!worker_context.empty())
		log::debug
		{
			log, "Waiting for %zu bridge workers...",
			worker_context.size(),
		};

	for(auto &worker : worker_context)
		worker.join();
}

void
ircd::m::bridge::worker(const std::string event_,
                        const std::string event_id)
try
{
	const m::event event
	{
		json::object{event_}, event_id
	};

	const bridge::config config
	{
		json::get<"content"_>(event)
	};

	const rfc3986::uri uri
	{
		at<"url"_>(config)
	};

	const unique_mutable_buffer buf
	{
		event::MAX_SIZE * 8
	};

	log::notice
	{
		log, "Bridging to '%s' via %s by %s",
		json::get<"id"_>(config),
		uri.remote,
		event_id,
	};

	run::barrier<ctx::interrupted> {};
	const bool prelinking
	{
		server::prelink(uri.remote)
	};

	if(!prelinking)
		log::warning
		{
			log, "Bridging to '%s' via %s may not be possible :%s",
			json::get<"id"_>(config),
			uri.remote,
			server::errmsg(uri.remote),
		};

	worker_loop(config, uri, buf);
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Worker failed to initialize :%s",
		e.what(),
	};
}

void
ircd::m::bridge::worker_loop(const config &config,
                             const rfc3986::uri &uri,
                             const mutable_buffer &buf)
try
{
	const net::hostport target
	{
		uri.remote
	};

	auto since {vm::sequence::retired}; do
    {
		worker_dock.wait([&since]
		{
			return since < vm::sequence::retired;
		});

		// Wait here if the bridge is down.
		while(unlikely(server::errant(target)))
		{
			log::error
			{
				log, "Waiting for '%s' at %s with error :%s",
				json::get<"id"_>(config),
				uri.remote,
				server::errmsg(target),
			};

			sleep(15s);
			continue;
		}

		const events::range range
		{
			since, vm::sequence::retired + 1
		};

		since = worker_handle(config, target, range, buf);
		assert(since >= range.first);
		assert(since <= range.second);

		// Prevent spin for retrying the same range on handled exception.
		if(unlikely(since == range.first))
		{
			sleep(15s);
			continue;
		}
	}
	while(run::level == run::level::RUN);
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Worker unhandled :%s",
		e.what(),
	};
}

ircd::m::event::idx
ircd::m::bridge::worker_handle(const config &config,
                               const net::hostport &target,
                               const events::range &range_,
                               window_buffer buf)
try
{
	size_t count {0};
	auto range {range_};
	buf([&config, &count, &range]
	(const mutable_buffer &buf)
	{
		json::stack out
		{
			buf
		};

		count += make_txn(config, out, range);
		return out.completed();
	});

	if(!count)
		return range.second;

	const json::object content
	{
		buf.completed()
	};

	buf = window_buffer
	{
		buf.remains()
	};

	// Generate URL for the PUT
	char uribuf[448], txnidbuf[64];
	const string_view url
	{
		make_uri(uribuf, config, fmt::sprintf
		{
			txnidbuf, "transactions/%lu", range.first,
		})
	};

	log::debug
	{
		log, "[%s] PUT txn:%lu:%lu (%lu:%lu) events:%zu",
		json::get<"id"_>(config),
		range.first,
		range.second,
		range_.first,
		range_.second,
		count,
	};

	// HTTP request sans
	http::request
	{
		buf,
		host(target),
		"PUT",
		url,
		size(string_view(content)),
		"application/json; charset=utf-8"_sv,
	};

	// Outputs from consumed buffer
	server::out out;
	out.head = buf.completed();
	out.content = content;

	// Inputs to remaining buffer
	server::in in;
	in.head = buf.remains();
	in.content = in.head;

	// Send to bridge
	server::request::opts sopts;
	server::request req
	{
		target, std::move(out), std::move(in), &sopts
	};

	// Recv response
	const auto code
	{
		req.get(seconds(timeout))
	};

	log::logf
	{
		log, log::level::DEBUG,
		"[%s] %u txn:%lu:%lu (%lu:%lu) events:%zu :%s",
		json::get<"id"_>(config),
		uint(code),
		range.first,
		range.second,
		range_.first,
		range_.second,
		count,
		http::status(code),
	};

	return range.second + 1;
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "worker handle range:%lu:%lu :%s",
		range_.first,
		range_.second,
		e.what(),
	};

	return range_.first;
}

namespace ircd::m::bridge
{
	static bool pick_alias(const config &, const event::idx &, const event &, const json::array &, const m::room::alias &);
	static bool pick_alias(const config &, const event::idx &, const event &, const room &, const json::array &);
	static bool pick_room(const config &, const event::idx &, const event &, const room &, const json::array &);
	static bool pick_user(const config &, const event::idx &, const event &, const json::array &, const m::user::id &);
	static bool pick_user(const config &, const event::idx &, const event &, const room &, const json::array &);
	static bool pick(const config &, const event::idx &, const event &);
	static bool append(const config &, json::stack::array &, events::range &, size_t &, const event::idx &, const event &);
}

size_t
ircd::m::bridge::make_txn(const config &config,
                          json::stack &out,
                          events::range &range)
{
	json::stack::object top
	{
		out
	};

	json::stack::array events
	{
		top, "events"
	};

	size_t count {0};
	m::events::for_each(m::events::range{range}, [&]
	(const event::idx &event_idx, const event &event)
	{
		if(!pick(config, event_idx, event))
			return true;

		if(!append(config, events, range, count, event_idx, event))
			return false;

		return true;
	});

	return count;
}

bool
ircd::m::bridge::append(const config &config,
                        json::stack::array &events,
                        events::range &range,
                        size_t &count,
                        const event::idx &event_idx,
                        const event &event)
{
	event::append::opts opts;
	opts.event_idx = &event_idx;
	opts.query_txnid = false;
	opts.query_prev_state = true;
	opts.query_redacted = false;
	event::append
	{
		events, event, opts
	};

	log::debug
	{
		log, "[%s] ADD %s in %s idx:%lu txn:%lu:%lu events:%zu buffer:%zu",
		json::get<"id"_>(config),
		string_view{event.event_id},
		json::get<"room_id"_>(event),
		event_idx,
		range.first,
		range.second,
		count,
		events.s->remaining(),
	};

	++count;
	range.second = event_idx;
	const bool sufficient_buffer
	{
		events.s->remaining() > event::MAX_SIZE + 16_KiB
	};

	return sufficient_buffer;
}

bool
ircd::m::bridge::pick(const config &config,
                      const event::idx &event_idx,
                      const event &event)
{
	const room room
	{
		json::get<"room_id"_>(event)
	};

	if(internal(room))
		return false;

	const bridge::namespaces &namespaces
	{
		json::get<"namespaces"_>(config)
	};

	return false
	|| pick_user(config, event_idx, event, room, json::get<"users"_>(namespaces))
	|| pick_room(config, event_idx, event, room, json::get<"rooms"_>(namespaces))
	|| pick_alias(config, event_idx, event, room, json::get<"aliases"_>(namespaces))
	;
}

bool
ircd::m::bridge::pick_user(const config &config,
                           const event::idx &event_idx,
                           const event &event,
                           const room &room,
                           const json::array &namespaces)
{
	const m::user::id &sender
	{
		json::get<"sender"_>(event)
	};

	// Bridged user is the sender
	if(pick_user(config, event_idx, event, namespaces, sender))
		return true;

	// Bridged user is target of a membership state transition
	if(json::get<"type"_>(event) == "m.room.member")
	{
		// event::conforms ensures this is always a valid user_id
		const user::id &state_key
		{
			json::get<"state_key"_>(event)
		};

		if(pick_user(config, event_idx, event, namespaces, state_key))
			return true;
	}

	const room::members members
	{
		room
	};

	// Bridged user is in the room.
	return true
	&& !namespaces.empty() // avoid this query io if there's nothing to match.
	&& !members.for_each("join", my_host(), [&](const id::user &user_id)
	{
		return !pick_user(config, event_idx, event, namespaces, user_id);
	});
}

bool
ircd::m::bridge::pick_user(const config &config,
                           const event::idx &event_idx,
                           const event &event,
                           const json::array &namespaces,
                           const user::id &user_id)
{
	for(const json::object object : namespaces)
	{
		const bridge::namespace_ ns
		{
			object
		};

		const globular_imatch match
		{
			json::get<"regex"_>(ns)
		};

		if(match(user_id))
			return true;
	}

	return false;
}

bool
ircd::m::bridge::pick_room(const config &config,
                           const event::idx &event_idx,
                           const event &event,
                           const room &room,
                           const json::array &namespaces)
{
	for(const json::object object : namespaces)
	{
		const bridge::namespace_ ns
		{
			object
		};

		const globular_imatch match
		{
			json::get<"regex"_>(ns)
		};

		if(match(room.room_id))
			return true;
	}

	return false;
}

bool
ircd::m::bridge::pick_alias(const config &config,
                            const event::idx &event_idx,
                            const event &event,
                            const room &room,
                            const json::array &namespaces)
{
	const m::room::aliases aliases
	{
		room
	};

	return true
	&& !namespaces.empty()
	&& !aliases.for_each(my_host(), [&](const room::alias &alias)
	{
		return !pick_alias(config, event_idx, event, namespaces, alias);
	});
}

bool
ircd::m::bridge::pick_alias(const config &config,
                            const event::idx &event_idx,
                            const event &event,
                            const json::array &namespaces,
                            const m::room::alias &room_alias)
{
	for(const json::object object : namespaces)
	{
		const bridge::namespace_ ns
		{
			object
		};

		const globular_imatch match
		{
			json::get<"regex"_>(ns)
		};

		if(match(room_alias))
			return true;
	}

	return false;
}
