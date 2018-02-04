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

decltype(ircd::m::vm::fronts)
ircd::m::vm::fronts
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

	//TODO: XXX
	const int64_t depth
	{
		-1
	};

	//TODO: XXX
	const string_view auth_events {};

	//TODO: XXX
	const string_view prev_events {};

	const json::iov::set set[]
	{
		{ event, { "auth_events",       auth_events                }},
		{ event, { "depth",             depth                      }},
		{ event, { "origin_server_ts",  ircd::time<milliseconds>() }},
		{ event, { "origin",            my_host()                  }},
		{ event, { "prev_events",       prev_events                }},
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
		throw BAD_JSON("Required event field: event_id");
	}

	if(unlikely(!json::get<"type"_>(event)))
	{
		assert(0);
		throw BAD_JSON("Required event field: type");
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
	bool check_fault_resume(eval &);
	void check_fault_throw(eval &);

	void write(eval &);

	enum fault evaluate(eval &, const event &);
	enum fault evaluate(eval &, const vector_view<const event> &);
}

decltype(ircd::m::vm::eval::default_opts)
ircd::m::vm::eval::default_opts
{};

ircd::m::vm::eval::eval()
:eval(default_opts)
{
}

ircd::m::vm::eval::eval(const struct opts &opts)
:opts{&opts}
{
}

enum ircd::m::vm::fault
ircd::m::vm::eval::operator()()
{
	assert(0);
	return fault::ACCEPT;
}

enum ircd::m::vm::fault
ircd::m::vm::eval::operator()(const json::array &events)
{
	std::vector<m::event> event;
	event.reserve(events.count());

	auto it(begin(events));
	for(; it != end(events); ++it)
		event.emplace_back(*it);

	return operator()(vector_view<const m::event>(event));
}

enum ircd::m::vm::fault
ircd::m::vm::eval::operator()(const json::vector &events)
{
	std::vector<m::event> event;
	event.reserve(events.count());

	auto it(begin(events));
	for(; it != end(events); ++it)
		event.emplace_back(*it);

	return operator()(vector_view<const m::event>(event));
}

enum ircd::m::vm::fault
ircd::m::vm::eval::operator()(const event &event)
{
	return operator()(vector_view<const m::event>(&event, 1));
}

enum ircd::m::vm::fault
ircd::m::vm::eval::operator()(const vector_view<const event> &events)
{
	static const size_t max
	{
		128
	};

	for(size_t i(0); i < events.size(); i += max)
	{
		const vector_view<const event> evs
		{
			events.data() + i, std::min(events.size() - i, size_t(max))
		};

		enum fault code;
		switch((code = evaluate(*this, evs)))
		{
			case fault::ACCEPT:
				continue;

			default:
				//check_fault_throw(*this);
				return code;
		}
	}

	return fault::ACCEPT;
}

enum ircd::m::vm::fault
ircd::m::vm::evaluate(eval &eval,
                      const vector_view<const event> &event)
{

	for(size_t i(0); i < event.size(); ++i)
	{
		if(evaluate(eval, event[i]) == fault::ACCEPT)
		{
			log.info("%s", pretty_oneline(event[i]));
			vm::inserted.notify(event[i]);
		}
	}

	write(eval);
	return fault::ACCEPT;
}

enum ircd::m::vm::fault
ircd::m::vm::evaluate(eval &eval,
                      const event &event)
{
	const auto &event_id
	{
		at<"event_id"_>(event)
	};

	const auto &depth
	{
		json::get<"depth"_>(event, 0)
	};

	const auto &room_id
	{
		json::get<"room_id"_>(event)
	};

	auto &front
	{
		fronts.get(room_id, event)
	};

	front.top = depth;

	auto code
	{
		fault::ACCEPT
	};

	dbs::write(event, eval.txn);
	return code;
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

/*
bool
ircd::m::vm::check_fault_resume(eval &eval)
{
	switch(fault)
	{
		case fault::ACCEPT:
			return true;

		default:
			check_fault_throw(eval);
			return false;
    }
}

void
ircd::m::vm::check_fault_throw(eval &eval)
{
	// With nothrow there is nothing else for this function to do as the user
	// will have to test the fault code and error ptr manually.
	if(eval.opts->nothrow)
		return;

	switch(fault)
	{
		case fault::ACCEPT:
			return;

		case fault::GENERAL:
			if(eval.error)
				std::rethrow_exception(eval.error);
			// [[fallthrough]]

		default:
			throw VM_FAULT("(%u): %s", uint(fault), reflect(fault));
    }
}
*/


ircd::ctx::view<const ircd::m::event>
ircd::m::vm::inserted
{};

uint64_t
ircd::m::vm::current_sequence
{};

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

bool
ircd::m::vm::test(const query<> &where)
{
	return test(where, [](const auto &event)
	{
		return true;
	});
}

bool
ircd::m::vm::test(const query<> &clause,
                  const closure_bool &closure)
{
	bool ret{false};
	dbs::_query(clause, [&ret, &closure](const auto &event)
	{
		ret = closure(event);
		return true;
 	});

	return ret;
}

bool
ircd::m::vm::until(const query<> &where)
{
	return until(where, [](const auto &event)
	{
		return true;
	});
}

bool
ircd::m::vm::until(const query<> &clause,
                   const closure_bool &closure)
{
	bool ret{true};
	dbs::_query(clause, [&ret, &closure](const auto &event)
	{
		ret = closure(event);
		return ret;
	});

	return ret;
}

size_t
ircd::m::vm::count(const query<> &where)
{
	return count(where, [](const auto &event)
	{
		return true;
	});
}

size_t
ircd::m::vm::count(const query<> &where,
                   const closure_bool &closure)
{
	size_t i(0);
	for_each(where, [&closure, &i](const auto &event)
	{
		i += closure(event);
	});

	return i;
}

void
ircd::m::vm::for_each(const closure &closure)
{
	const query<where::noop> where{};
	for_each(where, closure);
}

void
ircd::m::vm::for_each(const query<> &clause,
                      const closure &closure)
{
	dbs::_query(clause, [&closure](const auto &event)
	{
		closure(event);
		return false;
	});
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
