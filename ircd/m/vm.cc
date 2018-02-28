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

decltype(ircd::m::vm::inserted)
ircd::m::vm::inserted
{};

decltype(ircd::m::vm::current_sequence)
ircd::m::vm::current_sequence
{};

decltype(ircd::m::vm::default_opts)
ircd::m::vm::default_opts
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

namespace ircd::m::vm
{
	extern hook::site commit_hook;
}

decltype(ircd::m::vm::commit_hook)
ircd::m::vm::commit_hook
{
	{ "name", "vm commit" }
};

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

	//TODO: X
	vm::opts opts;
	opts.non_conform |= event::conforms::MISSING_PREV_STATE;

	vm::eval eval{opts};
	ircd::timer timer;

	commit_hook(event);
	eval(event);

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
	extern hook::site notify_hook;

	void _tmp_effects(const m::event &event); //TODO: X
	void write(eval &);
}

decltype(ircd::m::vm::notify_hook)
ircd::m::vm::notify_hook
{
	{ "name", "vm notify" }
};

ircd::m::vm::eval::eval(const vm::opts &opts)
:opts{&opts}
{
}

ircd::m::vm::eval::eval(const event &event,
                        const vm::opts &opts)
:opts{&opts}
{
	operator()(event);
}

enum ircd::m::vm::fault
ircd::m::vm::eval::operator()(const event &event)
try
{
	const event::conforms report
	{
		event, opts->non_conform.report
	};

	if(!report.clean())
		throw error
		{
			fault::INVALID, "Non-conforming event: %s", string(report)
		};

	const m::event::id &event_id
	{
		at<"event_id"_>(event)
	};

	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	if(!opts->replays && exists(event_id))  //TODO: exclusivity
		throw error
		{
			fault::EXISTS, "Event has already been evaluated."
		};


	const auto &depth
	{
		json::get<"depth"_>(event)
	};

	const auto &type
	{
		unquote(at<"type"_>(event))
	};

	const event::prev prev
	{
		event
	};

	const size_t prev_count
	{
		size(json::get<"prev_events"_>(prev))
	};

	//TODO: ex
	if(opts->write && prev_count)
	{
		for(size_t i(0); i < prev_count; ++i)
		{
			const auto prev_id{prev.prev_event(i)};
			if(opts->prev_check_exists && !dbs::exists(prev_id))
				throw error
				{
					fault::EVENT, "Missing prev event %s", string_view{prev_id}
				};
		}

		uint64_t top;
		const id::event::buf head
		{
			m::head(room_id, top)
		};

		m::room room{room_id, head};
		m::room::state state{room};
		m::state::id_buffer new_root_buf;
		const auto new_root
		{
			dbs::write(txn, new_root_buf, state.root_id, event)
		};
	}
	else if(opts->write)
	{
		m::state::id_buffer new_root_buf;
		const auto new_root
		{
			dbs::write(txn, new_root_buf, string_view{}, event)
		};
	}

	if(opts->write)
		write(*this);

	if(opts->notify)
	{
		notify_hook(event);
		vm::inserted.notify(event);
	}

	if(opts->effects)
		_tmp_effects(event);

	if(opts->debuglog_accept)
		log.debug("%s", pretty_oneline(event));

	if(opts->infolog_accept)
		log.info("%s", pretty_oneline(event));

	return fault::ACCEPT;
}
catch(const error &e)
{
	if(opts->errorlog & e.code)
		log.error("eval %s: %s %s",
		          json::get<"event_id"_>(event),
		          e.what(),
		          e.content);

	if(opts->warnlog & e.code)
		log.warning("eval %s: %s %s",
		            json::get<"event_id"_>(event),
		            e.what(),
		            e.content);

	if(opts->nothrows & e.code)
		return e.code;

	throw;
}
catch(const std::exception &e)
{
	if(opts->errorlog & fault::GENERAL)
		log.error("eval %s: #GP: %s",
		          json::get<"event_id"_>(event),
		          e.what());

	if(opts->warnlog & fault::GENERAL)
		log.warning("eval %s: #GP: %s",
		            json::get<"event_id"_>(event),
		            e.what());

	if(opts->nothrows & fault::GENERAL)
		return fault::GENERAL;

	throw error
	{
		fault::GENERAL, "%s", e.what()
	};
}

void
ircd::m::vm::write(eval &eval)
{
	log.debug("Committing %zu cells in %zu bytes to events database...",
	          eval.txn.size(),
	          eval.txn.bytes());

	eval.txn();
	vm::current_sequence++;
	eval.txn.clear();
}

ircd::string_view
ircd::m::vm::reflect(const enum fault &code)
{
	switch(code)
	{
		case fault::ACCEPT:       return "ACCEPT";
		case fault::EXISTS:       return "EXISTS";
		case fault::INVALID:      return "INVALID";
		case fault::DEBUGSTEP:    return "DEBUGSTEP";
		case fault::BREAKPOINT:   return "BREAKPOINT";
		case fault::GENERAL:      return "GENERAL";
		case fault::EVENT:        return "EVENT";
		case fault::STATE:        return "STATE";
	}

	return "??????";
}

//TODO: X
void
ircd::m::vm::_tmp_effects(const m::event &event)
{
	const m::room::id room_id{at<"room_id"_>(event)};
	const m::user::id sender{at<"sender"_>(event)};
	const auto &type{at<"type"_>(event)};

	//TODO: X
	/*
	if(type == "m.room.create" && my_host(user::id{at<"sender"_>(event)}.host()))
	{
		const m::room room{room::id{room_id}};
		send(room, at<"sender"_>(event), "m.room.power_levels", {}, json::members
		{
			{ "users", json::members
			{
				{ at<"sender"_>(event), 100L }
			}}
		});
	}
	*/

	//TODO: X
	if(type == "m.room.member" && my_host(sender.host()))
	{
		m::user::room user_room{sender};
		send(user_room, sender, "ircd.member", room_id, at<"content"_>(event));
	}

	//TODO: X
	if(type == "m.room.join_rules" && my_host(sender.host()))
	{
		send(room::id{"!public:zemos.net"}, sender, "ircd.room", room_id, {});
	}
}
