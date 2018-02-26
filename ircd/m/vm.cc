// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/m/m.h>

decltype(ircd::m::vm::log)
ircd::m::vm::log
{
	"vm", 'v'
};

ircd::ctx::view<const ircd::m::event>
ircd::m::vm::inserted
{};

uint64_t
ircd::m::vm::current_sequence
{};

/// This function takes an event object vector and adds our origin and event_id
/// and hashes and signature and attempts to inject the event into the core.
/// The caller is expected to have done their best to check if this event will
/// succeed once it hits the core because failures blow all this effort. The
/// caller's ircd::ctx will obviously yield for evaluation, which may involve
/// requests over the internet in the worst case. Nevertheless, the evaluation,
/// write and release sequence of the core commitment is designed to allow the
/// caller to service a usual HTTP request conveying success or error without
/// hanging their client too much.
///
ircd::m::event::id::buf
ircd::m::vm::commit(json::iov &event,
                    const json::iov &contents)
{
	const auto &room_id
	{
		event.at("room_id")
	};

	// derp
	const json::strung content
	{
		contents
	};

	const json::iov::set set[]
	{
		{ event, { "origin_server_ts",  ircd::time<milliseconds>() }},
		{ event, { "origin",            my_host()                  }},
	};

	thread_local char preimage_buf[64_KiB];
	const_buffer preimage
	{
		stringify(mutable_buffer{preimage_buf}, event)
	};

	sha256::buf hash
	{
		sha256{preimage}
	};

	event::id::buf eid_buf;
	const json::iov::set _event_id
	{
		event, { "event_id", m::event_id(event, eid_buf, hash) }
	};

	char hashes_buf[128];
	string_view hashes;
	{
		const json::iov::push _content
		{
			event, { "content", string_view{content} }
		};

		// derp
		preimage = stringify(mutable_buffer{preimage_buf}, event);
		hash = sha256{preimage};

		// derp
		thread_local char hashb64[hash.size() * 2];
		hashes = stringify(mutable_buffer{hashes_buf}, json::members
		{
			{ "sha256", b64encode_unpadded(hashb64, hash) }
		});
	}

	const json::iov::push _hashes
	{
		event, { "hashes", hashes }
	};

	// derp
	ed25519::sig sig;
	{
		const json::iov::push _content
		{
			event, { "content", "{}" }
		};

		preimage = stringify(mutable_buffer{preimage_buf}, event);
		sig = self::secret_key.sign(preimage);
		assert(self::public_key.verify(preimage, sig));
	}

	char sigb64[64];
	const json::members sigs
	{
		{ my_host(), json::members
		{
			json::member { self::public_key_id, b64encode_unpadded(sigb64, sig) }
		}}
	};

	const json::iov::push _final[]
	{
		{ event, { "content",     content  }},
		{ event, { "signatures",  sigs     }},
	};

	return commit(event);
}

/// Insert a new event originating from this server.
///
/// Figure 1:
///          in     .  <-- injection
///    ___:::::::__//
///    |  ||||||| //   <-- this function
///    |   \\|// //|
///    |    ||| // |   |  acceleration
///    |    |||//  |   |
///    |    |||/   |   |
///    |    |||    |   V
///    |    !!!    |
///    |     *     |   <----- nozzle
///    | ///|||\\\ |
///    |/|/|/|\|\|\|   <---- propagation cone
///  _/|/|/|/|\|\|\|\_
///         out
///
ircd::m::event::id::buf
ircd::m::vm::commit(json::iov &iov)
{
	const m::event event
	{
		iov
	};

	if(unlikely(!json::get<"event_id"_>(event)))
	{
		assert(0);
		throw m::BAD_JSON
		{
			"Required event field: event_id"
		};
	}

	if(unlikely(!json::get<"type"_>(event)))
	{
		assert(0);
		throw m::BAD_JSON
		{
			"Required event field: type"
		};
	}

	log.debug("injecting event(mark: %ld) %s",
	          vm::current_sequence,
	          pretty_oneline(event));

	ircd::timer timer;

	eval{event};

	log.debug("committed event %s (mark: %ld time: %ld$ms)",
	          at<"event_id"_>(event),
	          vm::current_sequence,
	          timer.at<milliseconds>().count());

	return unquote(at<"event_id"_>(event));
}

//
// Eval
//
// Processes any event from any place from any time and does whatever is
// necessary to validate, reject, learn from new information, ignore old
// information and advance the state of IRCd as best as possible.

namespace ircd::m::vm
{
	hook::site notify_hook
	{
		{ "name", "vm notify" }
	};

	void write(eval &);
}

decltype(ircd::m::vm::eval::default_opts)
ircd::m::vm::eval::default_opts
{};

ircd::m::vm::eval::eval(const struct opts &opts)
:opts{&opts}
{
}

ircd::m::vm::eval::eval(const event &event,
                        const struct opts &opts)
:opts{&opts}
{
	operator()(event);
}

enum ircd::m::vm::fault
ircd::m::vm::eval::operator()(const event &event)
try
{
	const auto &room_id
	{
		at<"room_id"_>(event)
	};

	const auto &event_id
	{
		at<"event_id"_>(event)
	};

	const auto &depth
	{
		at<"depth"_>(event)
	};

	const auto &type
	{
		unquote(at<"type"_>(event))
	};

	if(depth < 0)
		throw VM_INVALID
		{
			"Depth can never be negative"
		};

	if(depth == 0 && type != "m.room.create")
		throw VM_INVALID
		{
			"Depth can only be zero for m.room.create types"
		};

	const event::prev prev{event};
	const json::array prev_events
	{
		json::get<"prev_events"_>(prev)
	};

	const json::array prev0
	{
		prev_events[0]
	};

	const string_view previd
	{
		unquote(prev0[0])
	};

	char old_rootbuf[64];
	const auto old_root
	{
		previd? dbs::state_root(old_rootbuf, previd) : string_view{}
	};

	char new_rootbuf[64];
	const auto new_root
	{
		dbs::write(txn, new_rootbuf, old_root, event)
	};

	++cs;
	log.info("%s", pretty_oneline(event));

	write(*this);
	return fault::ACCEPT;
}
catch(const error &e)
{
	log.error("eval %s: %s %s",
	          json::get<"event_id"_>(event),
	          e.what(),
	          e.content);
	throw;
}

void
ircd::m::vm::write(eval &eval)
{
	log.debug("Committing %zu events to database...",
	          eval.cs);

	eval.txn();
	vm::current_sequence += eval.cs;
	eval.txn.clear();
	eval.cs = 0;
}

ircd::m::vm::front &
ircd::m::vm::fronts::get(const room::id &room_id,
                         const event &event)
try
{
	front &ret
	{
		map[std::string(room_id)]
	};

	if(ret.map.empty())
		fetch(room_id, ret, event);

	return ret;
}
catch(const std::exception &e)
{
	map.erase(std::string(room_id));
	throw;
}

ircd::m::vm::front &
ircd::m::vm::fronts::get(const room::id &room_id)
{
	const auto it
	{
		map.find(std::string(room_id))
	};

	if(it == end(map))
		throw m::NOT_FOUND
		{
			"No fronts for unknown room %s", room_id
		};

	front &ret{it->second};
	if(unlikely(ret.map.empty()))
		throw m::NOT_FOUND
		{
			"No fronts for room %s", room_id
		};

	return ret;
}

ircd::m::vm::front &
ircd::m::vm::fetch(const room::id &room_id,
                   front &front,
                   const event &event)
{
	const query<where::equal> query
	{
		{ "room_id", room_id },
	};

	for_each(query, [&front]
	(const auto &event)
	{
		for_each(m::event::prev{event}, [&front, &event]
		(const auto &key, const auto &prev_events)
		{
			for(const json::array &prev_event : prev_events)
			{
				const event::id &prev_event_id{unquote(prev_event[0])};
				front.map.erase(std::string{prev_event_id});
			}

			const auto &depth
			{
				json::get<"depth"_>(event)
			};

			if(depth > front.top)
				front.top = depth;

			front.map.emplace(at<"event_id"_>(event), depth);
		});
	});

	if(!front.map.empty())
		return front;

	const event::id &event_id
	{
		at<"event_id"_>(event)
	};

	if(!my_host(room_id.host()))
	{
		log.debug("No fronts available for %s; acquiring state eigenvalue at %s...",
		          string_view{room_id},
		          string_view{event_id});

		return front;
	}

	log.debug("No fronts available for %s using %s",
	          string_view{room_id},
	          string_view{event_id});

	front.map.emplace(at<"event_id"_>(event), json::get<"depth"_>(event));
	front.top = json::get<"depth"_>(event);

	if(!my_host(room_id.host()))
	{
		assert(0);
		//backfill(room_id, event_id, 64);
		return front;
	}

	return front;
}

ircd::string_view
ircd::m::vm::reflect(const enum fault &code)
{
	switch(code)
	{
		case fault::ACCEPT:       return "ACCEPT";
		case fault::GENERAL:      return "GENERAL";
		case fault::DEBUGSTEP:    return "DEBUGSTEP";
		case fault::BREAKPOINT:   return "BREAKPOINT";
		case fault::EVENT:        return "EVENT";
	}

	return "??????";
}
