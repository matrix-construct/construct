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

decltype(ircd::m::vm::accept)
ircd::m::vm::accept
{};

decltype(ircd::m::vm::current_sequence)
ircd::m::vm::current_sequence
{};

decltype(ircd::m::vm::default_opts)
ircd::m::vm::default_opts
{};

/// This function takes an event object vector and adds our origin and event_id
/// and hashes and signature and attempts to inject the event into the core.
///
ircd::m::event::id::buf
ircd::m::vm::commit(json::iov &event,
                    const json::iov &contents,
                    const opts &opts)
{
	const json::iov::add_if _origin
	{
		event, opts.origin,
		{
			"origin", my_host()
		}
	};

	const json::iov::add_if _origin_server_ts
	{
		event, opts.origin_server_ts,
		{
			"origin_server_ts", ircd::time<milliseconds>()
		}
	};

	const json::strung content
	{
		contents
	};

	// event_id

	sha256::buf event_id_hash;
	if(opts.event_id)
	{
		const json::iov::push _content
		{
			event, { "content", content },
		};

		thread_local char preimage_buf[64_KiB];
		event_id_hash = sha256
		{
			stringify(mutable_buffer{preimage_buf}, event)
		};
	}

	event::id::buf eid_buf;
	const string_view event_id
	{
		opts.event_id?
			m::event_id(event, eid_buf, event_id_hash):
			string_view{}
	};

	const json::iov::add_if _event_id
	{
		event, opts.event_id, { "event_id", event_id }
	};

	// hashes

	char hashes_buf[128];
	const string_view hashes
	{
		opts.hash?
			m::event::hashes(hashes_buf, event, content):
			string_view{}
	};

	const json::iov::add_if _hashes
	{
		event, opts.hash, { "hashes", hashes }
	};

	// sigs

	char sigs_buf[384];
	const string_view sigs
	{
		opts.sign?
			m::event::signatures(sigs_buf, event, contents):
			string_view{}
	};

	const json::iov::add_if _sigs
	{
		event, opts.sign, { "signatures",  sigs }
	};

	const json::iov::push _content
	{
		event, { "content", content },
	};

	return commit(event, opts);
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
///    ===:::::::==//
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
ircd::m::vm::commit(const event &event,
                    const opts &opts)
{
	if(opts.debuglog_precommit)
		log.debug("injecting event(mark: %ld) %s",
		          vm::current_sequence,
		          pretty_oneline(event));

	check_size(event);
	commit_hook(event);

	vm::opts opts_{opts};

	// Some functionality on this server may create an event on behalf
	// of remote users. It's safe for us to mask this here, but eval'ing
	// this event in any replay later will require special casing.
	opts_.non_conform |= event::conforms::MISMATCH_ORIGIN_SENDER;

	//TODO: X
	opts_.non_conform |= event::conforms::MISSING_PREV_STATE;

	eval
	{
		event, opts_
	};

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
	extern hook::site eval_hook;
	extern hook::site notify_hook;

	void _tmp_effects(const m::event &event); //TODO: X
	void write(eval &);
	fault _eval_edu(eval &, const event &);
	fault _eval_pdu(eval &, const event &);
}

decltype(ircd::m::vm::eval_hook)
ircd::m::vm::eval_hook
{
	{ "name", "vm.eval" }
};

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
	assert(opts);
	const event::conforms &report
	{
		opts->conforming && !opts->conformed?
			event::conforms{event, opts->non_conform.report}:
			opts->report
	};

	if(opts->conforming && !report.clean())
		throw error
		{
			fault::INVALID, "Non-conforming event: %s", string(report)
		};

	// A conforming (with lots of masks) event without an event_id is an EDU.
	const fault ret
	{
		json::get<"event_id"_>(event)?
			_eval_pdu(*this, event):
			_eval_edu(*this, event)
	};

	if(ret != fault::ACCEPT)
		return ret;

	vm::accepted accepted
	{
		event, opts, &report
	};

	if(opts->notify)
	{
		notify_hook(event);
		vm::accept.expose(accepted);
	}

	if(opts->effects)
		_tmp_effects(event);

	if(opts->debuglog_accept)
		log.debug("%s", pretty_oneline(event));

	if(opts->infolog_accept)
		log.info("%s", pretty_oneline(event));

	return ret;
}
catch(const error &e)
{
	if(opts->errorlog & e.code)
		log.error("eval %s: %s %s",
		          json::get<"event_id"_>(event)?: json::string{"<edu>"},
		          e.what(),
		          e.content);

	if(opts->warnlog & e.code)
		log.warning("eval %s: %s %s",
		            json::get<"event_id"_>(event)?: json::string{"<edu>"},
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
		          json::get<"event_id"_>(event)?: json::string{"<edu>"},
		          e.what());

	if(opts->warnlog & fault::GENERAL)
		log.warning("eval %s: #GP: %s",
		            json::get<"event_id"_>(event)?: json::string{"<edu>"},
		            e.what());

	if(opts->nothrows & fault::GENERAL)
		return fault::GENERAL;

	throw error
	{
		fault::GENERAL, "%s", e.what()
	};
}

enum ircd::m::vm::fault
ircd::m::vm::_eval_edu(eval &eval,
                       const event &event)
{
	eval_hook(event);
	return fault::ACCEPT;
}

enum ircd::m::vm::fault
ircd::m::vm::_eval_pdu(eval &eval,
                       const event &event)
{
	assert(eval.opts);
	const auto &opts
	{
		*eval.opts
	};

	const m::event::id &event_id
	{
		at<"event_id"_>(event)
	};

	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	if(!opts.replays && exists(event_id))  //TODO: exclusivity
		throw error
		{
			fault::EXISTS, "Event has already been evaluated."
		};

	eval_hook(event);

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
	if(opts.write && prev_count)
	{
		for(size_t i(0); i < prev_count; ++i)
		{
			const auto prev_id{prev.prev_event(i)};
			if(opts.prev_check_exists && !exists(prev_id))
				throw error
				{
					fault::EVENT, "Missing prev event %s", string_view{prev_id}
				};
		}

		int64_t top;
		const id::event::buf head
		{
			m::head(std::nothrow, room_id, top)
		};

		if(top < 0 && (opts.head_must_exist || opts.history))
			throw error
			{
				fault::STATE, "Found nothing for room %s", string_view{room_id}
			};

		m::room room{room_id, head};
		m::room::state state{room};
		m::state::id_buffer new_root_buf;
		m::dbs::write_opts wopts;
		wopts.root_in = state.root_id;
		wopts.root_out = new_root_buf;
		wopts.present = opts.present;
		wopts.history = opts.history;
		const auto new_root
		{
			dbs::write(eval.txn, event, wopts)
		};
	}
	else if(opts.write)
	{
		m::state::id_buffer new_root_buf;
		m::dbs::write_opts wopts;
		wopts.root_out = new_root_buf;
		wopts.present = opts.present;
		wopts.history = opts.history;
		const auto new_root
		{
			dbs::write(eval.txn, event, wopts)
		};
	}

	if(opts.write)
		write(eval);

	return fault::ACCEPT;
}

void
ircd::m::vm::write(eval &eval)
{
	if(eval.opts->debuglog_accept)
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
	if(type == "m.room.member")
	{
		const m::room::id room_id{at<"room_id"_>(event)};
		const m::user::id sender{at<"sender"_>(event)};

		//TODO: ABA / TXN
		if(!exists(sender))
			create(sender);

		//TODO: ABA / TXN
		m::user::room user_room{sender};
		send(user_room, sender, "ircd.member", room_id, at<"content"_>(event));
	}

	//TODO: X
	if(type == "m.room.join_rules")
	{
		const m::room::id room_id{at<"room_id"_>(event)};
		const m::user::id sender{at<"sender"_>(event)};
		if(my_host(sender.host()))
			send(room::id{"!public:zemos.net"}, sender, "ircd.room", room_id, {});
	}
}

//
// accepted
//

ircd::m::vm::accepted::accepted(const m::event &event,
                                const vm::opts *const &opts,
                                const event::conforms *const &report)
:m::event{event}
,context{ctx::current}
,opts{opts}
,report{report}
{
}
