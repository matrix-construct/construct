// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::vm
{
	extern log::log log;
	extern hook::site commit_hook;
	extern hook::site eval_hook;
	extern hook::site notify_hook;

	static void _tmp_effects(const m::event &event); //TODO: X
	static void write(eval &);
	static fault _eval_edu(eval &, const event &);
	static fault _eval_pdu(eval &, const event &);

	extern "C" fault eval__event(eval &, const event &);
	extern "C" fault eval__commit(eval &, json::iov &, const json::iov &);
	extern "C" fault eval__commit_room(eval &, const room &, json::iov &, const json::iov &);

	static void init();
	static void fini();
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix Virtual Machine",
	ircd::m::vm::init, ircd::m::vm::fini
};

decltype(ircd::m::vm::log)
ircd::m::vm::log
{
	"vm", 'v'
};

decltype(ircd::m::vm::commit_hook)
ircd::m::vm::commit_hook
{
	{ "name", "vm.commit" }
};

decltype(ircd::m::vm::eval_hook)
ircd::m::vm::eval_hook
{
	{ "name", "vm.eval" }
};

decltype(ircd::m::vm::notify_hook)
ircd::m::vm::notify_hook
{
	{ "name", "vm.notify" }
};

//
// init
//

void
ircd::m::vm::init()
{
	id::event::buf event_id;
	current_sequence = retired_sequence(event_id);

	log::info
	{
		log, "BOOT %s @%lu [%s]",
		string_view{m::my_node.node_id},
		current_sequence,
		current_sequence? string_view{event_id} : "NO EVENTS"_sv
	};
}

void
ircd::m::vm::fini()
{
	id::event::buf event_id;
	const auto current_sequence
	{
		retired_sequence(event_id)
	};

	log::info
	{
		log, "HLT '%s' @%lu [%s]",
		string_view{m::my_node.node_id},
		current_sequence,
		current_sequence? string_view{event_id} : "NO EVENTS"_sv
	};
}

//
// eval
//

enum ircd::m::vm::fault
ircd::m::vm::eval__commit_room(eval &eval,
                               const room &room,
                               json::iov &event,
                               const json::iov &contents)
{
	assert(eval.issue);
	assert(eval.room_id);
	assert(eval.copts);
	assert(eval.opts);
	assert(room.room_id);

	const auto &opts
	{
		*eval.copts
	};

	const json::iov::push room_id
	{
		event, { "room_id", room.room_id }
	};

	int64_t depth{-1};
	event::id::buf prev_event_id
	{
	};

	if(room.event_id)
		prev_event_id = room.event_id;
	else
		std::tie(prev_event_id, depth, std::ignore) = m::top(std::nothrow, room.room_id);

	//TODO: X
	const event::fetch evf
	{
		prev_event_id, std::nothrow
	};

	if(room.event_id)
		depth = at<"depth"_>(evf);

	//TODO: X
	const json::iov::set_if depth_
	{
		event, !event.has("depth"),
		{
			"depth", depth + 1
		}
	};

	char ae_buf[512];
	json::array auth_events
	{
		json::get<"auth_events"_>(evf)?
			json::get<"auth_events"_>(evf):
			json::array{"[]"}
	};

	if(depth != -1)
	{
		std::vector<json::value> ae;

		room.get(std::nothrow, "m.room.create", "", [&ae]
		(const m::event &event)
		{
			const json::value ae0[]
			{
				{ json::get<"event_id"_>(event) },
				{ json::get<"hashes"_>(event)   }
			};

			const json::value ae_
			{
				ae0, 2
			};

			ae.emplace_back(ae_);
		});

		room.get(std::nothrow, "m.room.join_rules", "", [&ae]
		(const m::event &event)
		{
			const json::value ae0[]
			{
				{ json::get<"event_id"_>(event) },
				{ json::get<"hashes"_>(event)   }
			};

			const json::value ae_
			{
				ae0, 2
			};

			ae.emplace_back(ae_);
		});

		auth_events = json::stringify(mutable_buffer{ae_buf}, json::value
		{
			ae.data(), ae.size()
		});

		room.get(std::nothrow, "m.room.power_levels", "", [&ae]
		(const m::event &event)
		{
			const json::value ae0[]
			{
				{ json::get<"event_id"_>(event) },
				{ json::get<"hashes"_>(event)   }
			};

			const json::value ae_
			{
				ae0, 2
			};

			ae.emplace_back(ae_);
		});

		auth_events = json::stringify(mutable_buffer{ae_buf}, json::value
		{
			ae.data(), ae.size()
		});

		if(event.at("type") != "m.room.member")
		room.get(std::nothrow, "m.room.member", event.at("sender"), [&ae]
		(const m::event &event)
		{
			const json::value ae0[]
			{
				{ json::get<"event_id"_>(event) },
				{ json::get<"hashes"_>(event)   }
			};

			const json::value ae_
			{
				ae0, 2
			};

			ae.emplace_back(ae_);
		});

		auth_events = json::stringify(mutable_buffer{ae_buf}, json::value
		{
			ae.data(), ae.size()
		});
	}

	//TODO: X
	const auto &prev_state
	{
		json::get<"prev_state"_>(evf)?
			json::get<"prev_state"_>(evf):
			json::array{"[]"}
	};
/*
	const event::fetch pvf
	{
		prev_event_id, std::nothrow
	};
*/
	//TODO: X
	json::value prev_event0[]
	{
		{ string_view{prev_event_id}  },
		{ json::get<"hashes"_>(evf) }
	};

	//TODO: X
	json::value prev_event
	{
		prev_event0, empty(prev_event_id)? 0UL: 2UL
	};

	//TODO: X
	json::value prev_events_
	{
		&prev_event, !empty(prev_event_id)
	};

	std::string prev_events
	{
		json::strung(prev_events_)
	};

	//TODO: X
	const json::iov::push prevs[]
	{
		{ event, { "auth_events",  auth_events  } },
		{ event, { "prev_events",  prev_events  } },
		{ event, { "prev_state",   prev_state   } },
	};

	return eval(event, contents);
}

enum ircd::m::vm::fault
ircd::m::vm::eval__commit(eval &eval,
                          json::iov &event,
                          const json::iov &contents)
{
	assert(eval.issue);
	assert(eval.copts);
	assert(eval.opts);
	assert(eval.copts);

	const auto &opts
	{
		*eval.copts
	};

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

	const string_view event_id
	{
		opts.event_id?
			make_id(event, eval.event_id, event_id_hash):
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

	return eval(event);
}

enum ircd::m::vm::fault
ircd::m::vm::eval__event(eval &eval,
                         const event &event)
try
{
	assert(eval.opts);
	assert(eval.event_);
	assert(eval.id);
	assert(eval.ctx);

	const auto &opts
	{
		*eval.opts
	};

	if(eval.copts)
	{
		if(unlikely(!my_host(at<"origin"_>(event))))
			throw error
			{
				fault::GENERAL, "Committing event for origin: %s", at<"origin"_>(event)
			};

		if(eval.copts->debuglog_precommit)
			log.debug("injecting event(mark +%ld) %s",
			          vm::current_sequence,
			          pretty_oneline(event));

		check_size(event);
		commit_hook(event);
	}

	const event::conforms &report
	{
		opts.conforming && !opts.conformed?
			event::conforms{event, opts.non_conform.report}:
			opts.report
	};

	if(opts.conforming && !report.clean())
		throw error
		{
			fault::INVALID, "Non-conforming event: %s", string(report)
		};

	// A conforming (with lots of masks) event without an event_id is an EDU.
	const fault ret
	{
		json::get<"event_id"_>(event)?
			_eval_pdu(eval, event):
			_eval_edu(eval, event)
	};

	if(ret != fault::ACCEPT)
		return ret;

	vm::accepted accepted
	{
		event, &opts, &report
	};

	if(opts.effects)
		notify_hook(event);

	if(opts.notify)
		vm::accept(accepted);

	if(opts.effects)
		_tmp_effects(event);

	if(opts.debuglog_accept)
		log.debug("%s", pretty_oneline(event));

	if(opts.infolog_accept)
		log.info("%s", pretty_oneline(event));

	return ret;
}
catch(const error &e)
{
	if(eval.opts->errorlog & e.code)
		log::error
		{
			log, "eval %s: %s %s",
			json::get<"event_id"_>(event)?: json::string{"<edu>"},
			e.what(),
			e.content
		};

	if(eval.opts->warnlog & e.code)
		log::warning
		{
			log, "eval %s: %s %s",
			json::get<"event_id"_>(event)?: json::string{"<edu>"},
			e.what(),
			e.content
		};

	if(eval.opts->nothrows & e.code)
		return e.code;

	throw;
}
catch(const ctx::interrupted &e)
{
	if(eval.opts->errorlog & fault::INTERRUPT)
		log::error
		{
			log, "eval %s: #NMI: %s",
			json::get<"event_id"_>(event)?: json::string{"<edu>"},
			e.what()
		};

	if(eval.opts->warnlog & fault::INTERRUPT)
		log::warning
		{
			log, "eval %s: #NMI: %s",
			json::get<"event_id"_>(event)?: json::string{"<edu>"},
			e.what()
		};

	throw;
}
catch(const std::exception &e)
{
	if(eval.opts->errorlog & fault::GENERAL)
		log::error
		{
			log, "eval %s: #GP: %s",
			json::get<"event_id"_>(event)?: json::string{"<edu>"},
			e.what()
		};

	if(eval.opts->warnlog & fault::GENERAL)
		log::warning
		{
			log, "eval %s: #GP: %s",
			json::get<"event_id"_>(event)?: json::string{"<edu>"},
			e.what()
		};

	if(eval.opts->nothrows & fault::GENERAL)
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

	if(opts.verify)
		if(!verify(event))
			throw m::BAD_SIGNATURE
			{
				"Signature verification failed"
			};

	const size_t reserve_bytes
	{
		opts.reserve_bytes == size_t(-1)?
			json::serialized(event):
			opts.reserve_bytes
	};

	db::txn txn
	{
		*dbs::events, db::txn::opts
		{
			reserve_bytes + opts.reserve_index,   // reserve_bytes
			0,                                    // max_bytes (no max)
		}
	};

	eval.txn = &txn;
	const unwind cleartxn{[&eval]
	{
		eval.txn = nullptr;
	}};

	// Obtain sequence number here
	eval.sequence = ++vm::current_sequence;

	m::dbs::write_opts wopts;
	wopts.present = opts.present;
	wopts.history = opts.history;
	wopts.head = opts.head;
	wopts.refs = opts.refs;
	wopts.idx = eval.sequence;

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
		id::event::buf head;
		std::tie(head, top, std::ignore) = m::top(std::nothrow, room_id);
		if(top < 0 && (opts.head_must_exist || opts.history))
			throw error
			{
				fault::STATE, "Found nothing for room %s", string_view{room_id}
			};

		m::room room{room_id, head};
		m::room::state state{room};
		m::state::id_buffer new_root_buf;
		wopts.root_in = state.root_id;
		wopts.root_out = new_root_buf;
		const auto new_root
		{
			dbs::write(*eval.txn, event, wopts)
		};
	}
	else if(opts.write)
	{
		m::state::id_buffer new_root_buf;
		wopts.root_out = new_root_buf;
		const auto new_root
		{
			dbs::write(*eval.txn, event, wopts)
		};
	}

	if(opts.write)
		write(eval);

	return fault::ACCEPT;
}

void
ircd::m::vm::write(eval &eval)
{
	auto &txn(*eval.txn);
	if(eval.opts->debuglog_accept)
		log::debug
		{
			log, "Committing %zu cells in %zu bytes to events database...",
			txn.size(),
			txn.bytes()
		};

	txn();
}

uint64_t
ircd::m::vm::retired_sequence()
{
	event::id::buf event_id;
	return retired_sequence(event_id);
}

uint64_t
ircd::m::vm::retired_sequence(event::id::buf &event_id)
{
	static constexpr auto column_idx
	{
		json::indexof<event, "event_id"_>()
	};

	auto &column
	{
		dbs::event_column.at(column_idx)
	};

	const auto it
	{
		column.rbegin()
	};

	if(!it)
	{
		// If this iterator is invalid the events db should
		// be completely fresh.
		assert(db::sequence(*dbs::events) == 0);
		return 0;
	}

	const auto &ret
	{
		byte_view<uint64_t>(it->first)
	};

	event_id = it->second;
	return ret;
}

//TODO: X
void
ircd::m::vm::_tmp_effects(const m::event &event)
{
	const auto &type{at<"type"_>(event)};

	//TODO: X
	if(type == "m.room.join_rules")
	{
		const m::room::id room_id{at<"room_id"_>(event)};
		const m::user::id sender{at<"sender"_>(event)};
		if(my_host(sender.host()))
			send(room::id{"!public:zemos.net"}, sender, "ircd.room", room_id, {});
	}

	//TODO: X
	if(type == "m.room.create")
	{
		const string_view local{m::room::id{at<"room_id"_>(event)}.localname()};
		if(local != "users") //TODO: circ dep
			send(my_room, at<"sender"_>(event), "ircd.room", at<"room_id"_>(event), json::object{});
	}
}
