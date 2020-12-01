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
	template<class... args> static fault handle_error(const opts &, const fault &, const string_view &fmt, args&&... a);
	template<class T> static void call_hook(hook::site<T> &, eval &, const event &, T&& data);
	static size_t calc_txn_reserve(const opts &, const event &);
	static void write_commit(eval &);
	static void write_append(eval &, const event &);
	static fault execute_edu(eval &, const event &);
	static fault execute_pdu(eval &, const event &);
	static fault execute_du(eval &, const event &);
	static fault execute(eval &, const event &);
	static fault inject3(eval &, json::iov &, const json::iov &);
	static fault inject1(eval &, json::iov &, const json::iov &);
	static void fini();
	static void init();

	extern hook::site<eval &> issue_hook;        ///< Called when this server is issuing event
	extern hook::site<eval &> conform_hook;      ///< Called for static evaluations of event
	extern hook::site<eval &> access_hook;       ///< Called for access control checking
	extern hook::site<eval &> fetch_auth_hook;   ///< Called to resolve dependencies
	extern hook::site<eval &> fetch_prev_hook;   ///< Called to resolve dependencies
	extern hook::site<eval &> fetch_state_hook;  ///< Called to resolve dependencies
	extern hook::site<eval &> eval_hook;         ///< Called for final event evaluation
	extern hook::site<eval &> post_hook;         ///< Called to apply effects pre-notify
	extern hook::site<eval &> notify_hook;       ///< Called to broadcast successful eval
	extern hook::site<eval &> effect_hook;       ///< Called to apply effects post-notify

	extern conf::item<bool> log_commit_debug;
	extern conf::item<bool> log_accept_debug;
	extern conf::item<bool> log_accept_info;
}

decltype(ircd::m::vm::log_commit_debug)
ircd::m::vm::log_commit_debug
{
	{ "name",     "ircd.m.vm.log.commit.debug" },
	{ "default",  true                         },
};

decltype(ircd::m::vm::log_accept_debug)
ircd::m::vm::log_accept_debug
{
	{ "name",     "ircd.m.vm.log.accept.debug" },
	{ "default",  true                         },
};

decltype(ircd::m::vm::log_accept_info)
ircd::m::vm::log_accept_info
{
	{ "name",     "ircd.m.vm.log.accept.info" },
	{ "default",  false                       },
};

decltype(ircd::m::vm::issue_hook)
ircd::m::vm::issue_hook
{
	{ "name", "vm.issue" }
};

decltype(ircd::m::vm::conform_hook)
ircd::m::vm::conform_hook
{
	{ "name", "vm.conform" }
};

decltype(ircd::m::vm::access_hook)
ircd::m::vm::access_hook
{
	{ "name", "vm.access" }
};

decltype(ircd::m::vm::fetch_auth_hook)
ircd::m::vm::fetch_auth_hook
{
	{ "name", "vm.fetch.auth" }
};

decltype(ircd::m::vm::fetch_prev_hook)
ircd::m::vm::fetch_prev_hook
{
	{ "name", "vm.fetch.prev" }
};

decltype(ircd::m::vm::fetch_state_hook)
ircd::m::vm::fetch_state_hook
{
	{ "name", "vm.fetch.state" }
};

decltype(ircd::m::vm::eval_hook)
ircd::m::vm::eval_hook
{
	{ "name", "vm.eval" }
};

decltype(ircd::m::vm::post_hook)
ircd::m::vm::post_hook
{
	{ "name", "vm.post" }
};

decltype(ircd::m::vm::notify_hook)
ircd::m::vm::notify_hook
{
	{ "name",        "vm.notify"  },
	{ "exceptions",  false        },
	{ "interrupts",  false        },
};

decltype(ircd::m::vm::effect_hook)
ircd::m::vm::effect_hook
{
	{ "name",        "vm.effect"  },
	{ "exceptions",  false        },
	{ "interrupts",  false        },
};

//
// execute
//

ircd::m::vm::fault
ircd::m::vm::execute(eval &eval,
                     const json::array &pdus)
{
	assert(eval.opts);
	const auto &opts
	{
		*eval.opts
	};

	auto it(begin(pdus));
	const bool empty
	{
		it == end(pdus)
	};

	if(unlikely(empty))
		return fault::ACCEPT;

	// Determine iff pdus.size()==1 without iterating the whole array
	const bool single
	{
		std::next(begin(pdus)) == end(pdus)
	};

	if(likely(single))
	{
		const m::event event
		{
			json::object(*it)
		};

		return execute(eval, vector_view(&event, 1));
	}

	fault ret{fault::ACCEPT};
	std::vector<m::event> eventv(64); do
	{
		size_t i(0);
		for(; i < eventv.size() && it != end(pdus); ++i, ++it)
			eventv[i] = json::object(*it);

		if(likely(!opts.ordered))
			std::sort(begin(eventv), begin(eventv) + i);

		assert(i <= eventv.size());
		execute(eval, vector_view(eventv.data(), i)); //XXX
	}
	while(it != end(pdus) && eval.evaluated < opts.limit);

	return ret;
}

ircd::m::vm::fault
ircd::m::vm::execute(eval &eval,
                     const vector_view<const event> &events)
{
	assert(eval.opts);
	const auto &opts
	{
		*eval.opts
	};

	const scope_restore eval_pdus
	{
		eval.pdus, events
	};

	if(likely(opts.phase[phase::VERIFY] && opts.mfetch_keys))
		if(events.size() > 1)
			eval.mfetch_keys();

	size_t accepted(0), existed(0), i, j, k;
	for(i = 0; i < events.size(); i += j)
	{
		id::event ids[64];
		for(j = 0; j < 64 && i + j < events.size() && eval.evaluated + j < opts.limit; ++j)
			ids[j] = events[i + j].event_id;

		// Bitset indicating which events already exist.
		const uint64_t exists
		{
			!opts.replays?
				m::exists(vector_view<const id::event>(ids, j)):
				0UL
		};

		const auto num_exists
		{
			__builtin_popcountl(exists)
		};

		existed += num_exists;
		eval.faulted + num_exists;
		for(k = 0; k < j; ++k, ++eval.evaluated) try
		{
			if(exists & (1 << k))
				continue;

			const auto status
			{
				execute(eval, events[i + k])
			};

			accepted += status == fault::ACCEPT;
			eval.accepted += status == fault::ACCEPT;
			eval.faulted += status != fault::ACCEPT;
		}
		catch(const ctx::interrupted &)
		{
			++eval.faulted;
			throw;
		}
		catch(const std::exception &)
		{
			++eval.faulted;
			continue;
		}
		catch(...)
		{
			++eval.faulted;
			throw;
		}
	}

	return fault::ACCEPT;
}

ircd::m::vm::fault
ircd::m::vm::execute(eval &eval,
                     const event &event)
try
{
	// This assertion is tripped if the end of your context's stack is
	// danger close; try increasing your stack size.
	const ctx::stack_usage_assertion sua;

	// m::vm bookkeeping that someone entered this function
	const scope_count executing
	{
		eval::executing
	};

	const scope_restore eval_phase
	{
		eval.phase, phase::EXECUTE
	};

	const scope_notify notify
	{
		vm::dock
	};

	assert(eval.opts);
	const auto &opts
	{
		*eval.opts
	};

	// Determine if this is an internal room creation event
	const bool is_internal_room_create
	{
		json::get<"type"_>(event) == "m.room.create" &&
		json::get<"sender"_>(event) &&
		m::myself(json::get<"sender"_>(event))
	};

	// Query for whether the room apropos is an internal room.
	const scope_restore room_internal
	{
		eval.room_internal,

		// Retain any existing true value from predetermination.
		eval.room_internal?
			eval.room_internal:

		// Case for creating an internal room (as a query will fail)
		is_internal_room_create?
			true:

		// Query to find out if internal room.
		json::get<"room_id"_>(event) && my(room::id(json::get<"room_id"_>(event)))?
			m::internal(json::get<"room_id"_>(event)):

		// Default to false
		false
	};

	const scope_restore eval_report
	{
		eval.report, event::conforms{}
	};

	const scope_restore eval_redacted
	{
		eval.redacted, false
	};

	// These checks only require the event data itself.
	if(likely(opts.phase[phase::CONFORM]) && !opts.edu)
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::CONFORM
		};

		const ctx::critical_assertion ca;
		call_hook(conform_hook, eval, event, eval);
	}

	assert(!eval.buf || size(eval.buf) >= event::MAX_SIZE);
	const bool redacted
	{
		eval.redacted
		|| eval.report.has(event::conforms::MISMATCH_HASHES)
	};

	if(!opts.edu && !eval.buf && (!opts.json_source || redacted))
		eval.buf = unique_mutable_buffer
		{
			event::MAX_SIZE, 64
		};

	const json::object event_source
	{
		// Canonize and redact from some other serialized source.
		!opts.edu && !opts.json_source && event.source && redacted?
			json::stringify(mutable_buffer{eval.buf}, m::essential(event.source, event::buf[0])):

		// Canonize and redact from no source.
		!opts.edu && !opts.json_source && redacted?
			json::stringify(mutable_buffer{eval.buf}, m::essential(event, event::buf[0])):

		// Canonize from some other serialized source.
		likely(!opts.edu && !opts.json_source && event.source)?
			json::stringify(mutable_buffer{eval.buf}, event.source):

		// Canonize from no source; usually taken when my(event).
		// XXX elision conditions go here
		likely(!opts.edu && !opts.json_source)?
			json::stringify(mutable_buffer{eval.buf}, event):

		// Use the input directly.
		string_view{event.source}
	};

	m::event _event_
	{
		event_source
	};

	const auto &_event
	{
		event_source? _event_: event
	};

	const scope_restore eval_room_id
	{
		eval.room_id,

		// Reassigns reference after any prior rewrites
		likely(json::get<"room_id"_>(_event))?
			string_view(json::get<"room_id"_>(_event)):

		// No action
		eval.room_id,
	};

	// Procure the room version.
	char room_version_buf[room::VERSION_MAX_SIZE];
	const scope_restore eval_room_version
	{
		eval.room_version,

		// The room version was supplied by the user in the options structure
		// because they know better.
		eval.opts->room_version?
			eval.opts->room_version:

		// The room version was already computed; probably by vm::inject().
		eval.room_version?
			eval.room_version:

		// There's no room version because there's no room!
		!eval.room_id?
			string_view{}:

		// Special case for create event
		json::get<"type"_>(event) == "m.room.create"?
			json::string(json::get<"content"_>(_event).get("room_version", "1"_sv)):

		// Special case for v1 distinguishable event_id's
		_event.event_id && _event.event_id.version() == "1"_sv?
			"1"_sv:

		// Make a query for the room version into the stack buffer.
		m::version(room_version_buf, room{eval.room_id}, std::nothrow)
	};

	// Copy the event_id into the eval buffer
	eval.event_id =
	{
		!opts.edu && !_event.event_id && eval.room_version == "3"?
			event::id::buf{event::id::v3{eval.event_id, _event}}:

		!opts.edu && !_event.event_id?
			event::id::buf{event::id::v4{eval.event_id, _event}}:

		!opts.edu?
			event::id::buf{_event.event_id}:

		event::id::buf{}
	};

	// Set a member pointer to the event currently being evaluated. This
	// allows other parallel evals to have deep access to this eval. It also
	// will be used to count this event as currently being evaluated.
	assert(!eval.event_);
	const scope_restore eval_event
	{
		eval.event_, &_event
	};

	mutable_cast(eval.event_)->event_id = eval.event_id;

	// If the event is already being evaluated, wait here until the other
	// evaluation is finished. If the other was successful, the exists()
	// check will skip this, otherwise we have to try again here because
	// this evaluator might be using different options/credentials.
	if(likely(opts.phase[phase::DUPCHK] && opts.unique) && _event.event_id)
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::DUPCHK
		};

		sequence::dock.wait([&_event]
		{
			return eval::count(_event.event_id) <= 1;
		});
	}

	// We can elide a lot of grief here by not proceeding further and simply
	// returning fault::EXISTS after an existence check. If we had to wait for
	// a duplicate eval this check will indicate its success.
	if(likely(!opts.replays && opts.nothrows & fault::EXISTS) && _event.event_id)
		if(!eval.copts && m::exists(_event.event_id))
			return fault::EXISTS;

	return execute_du(eval, _event);
}
catch(const vm::error &e)
{
	throw; // propagate from execute_du
}
catch(const m::error &e)
{
	const json::object &content
	{
		e.content
	};

	assert(eval.opts);
	return handle_error
	(
		*eval.opts, fault::GENERAL,
		"eval %s %s :%s :%s :%s",
		event.event_id?
			string_view{event.event_id}:
			"<unknown>"_sv,
		json::get<"room_id"_>(event)?
			string_view{json::get<"room_id"_>(event)}:
			"<unknown>"_sv,
		e.what(),
		json::string{content["errcode"]},
		json::string{content["error"]}
	);
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	assert(eval.opts);
	return handle_error
	(
		*eval.opts, fault::GENERAL,
		"eval %s %s (General Protection) :%s",
		event.event_id?
			string_view{event.event_id}:
			"<unknown>"_sv,
		json::get<"room_id"_>(event)?
			string_view{json::get<"room_id"_>(event)}:
			"<unknown>"_sv,
		e.what()
	);
}

ircd::m::vm::fault
ircd::m::vm::execute_du(eval &eval,
                        const event &event)
try
{
	assert(eval.id);
	assert(eval.ctx);
	assert(eval.opts);
	assert(eval.opts->edu || event.event_id);
	assert(eval.opts->edu || eval.event_id);
	assert(eval.event_id == event.event_id);
	assert(eval.event_);
	const auto &opts
	{
		*eval.opts
	};

	const scope_restore eval_sequence
	{
		eval.sequence, 0UL
	};

	// The issue hook is only called when this server is injecting a newly
	// created event.
	if(opts.phase[phase::ISSUE] && eval.copts && eval.copts->issue)
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::ISSUE
		};

		call_hook(issue_hook, eval, event, eval);
	}

	// Branch on whether the event is an EDU or a PDU
	const fault ret
	{
		event.event_id?
			execute_pdu(eval, event):
			execute_edu(eval, event)
	};

	// ret can be a fault code if the user masked the exception from being
	// thrown. If there's an error code here nothing further is done.
	if(ret != fault::ACCEPT)
		return ret;

	if(opts.debuglog_accept || bool(log_accept_debug))
		log::debug
		{
			log, "%s", pretty_oneline(event)
		};

	// The event was executed; now we broadcast the good news. This will
	// include notifying client `/sync` and the federation sender.
	if(likely(opts.phase[phase::NOTIFY]))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::NOTIFY
		};

		call_hook(notify_hook, eval, event, eval);
	}

	// The "effects" of the event are created by listeners on the effect hook.
	// These can include the creation of even more events, such as creating a
	// PDU out of an EDU, etc. Unlike the post_hook in execute_pdu(), the
	// notify for the event at issue here has already been made.
	if(likely(opts.phase[phase::EFFECTS]))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::EFFECTS
		};

		call_hook(effect_hook, eval, event, eval);
	}

	if(opts.infolog_accept || bool(log_accept_info))
		log::info
		{
			log, "%s", pretty_oneline(event)
		};

	return ret;
}
catch(const vm::error &e) // VM FAULT CODE
{
	const json::object &content{e.content};
	const json::string &error
	{
		content["error"]
	};

	return handle_error
	(
		*eval.opts, e.code,
		"execute %s %s :%s",
		event.event_id?
			string_view{event.event_id}:
			"<edu>"_sv,
		eval.room_id?
			eval.room_id:
			"<edu>"_sv,
		error
	);
}
catch(const m::error &e) // GENERAL MATRIX ERROR
{
	const json::object &content{e.content};
	const json::string error[]
	{
		content["errcode"],
		content["error"]
	};

	return handle_error
	(
		*eval.opts, fault::GENERAL,
		"execute %s %s :%s :%s",
		event.event_id?
			string_view{event.event_id}:
			"<edu>"_sv,
		eval.room_id?
			eval.room_id:
			"<edu>"_sv,
		error[0],
		error[1]
	);
}
catch(const ctx::interrupted &e) // INTERRUPTION
{
	throw;
}
catch(const std::exception &e) // ALL OTHER ERRORS
{
	return handle_error
	(
		*eval.opts, fault::GENERAL,
		"execute %s %s (General Protection) :%s",
		event.event_id?
			string_view{event.event_id}:
			"<edu>"_sv,
		eval.room_id?
			eval.room_id:
			"<edu>"_sv,
		e.what()
	);
}

ircd::m::vm::fault
ircd::m::vm::execute_edu(eval &eval,
                         const event &event)
{
	if(likely(eval.opts->phase[phase::EVALUATE]))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::EVALUATE
		};

		call_hook(eval_hook, eval, event, eval);
	}

	if(likely(eval.opts->phase[phase::POST]))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::POST
		};

		call_hook(post_hook, eval, event, eval);
	}

	return fault::ACCEPT;
}

ircd::m::vm::fault
ircd::m::vm::execute_pdu(eval &eval,
                         const event &event)
{
	const scope_count pending
	{
		sequence::pending
	};

	const scope_notify sequence_dock
	{
		sequence::dock, scope_notify::all
	};

	assert(eval.opts);
	const auto &opts
	{
		*eval.opts
	};

	const m::event::id &event_id
	{
		event.event_id
	};

	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const string_view &type
	{
		at<"type"_>(event)
	};

	const bool authenticate
	{
		opts.auth && !eval.room_internal
	};

	if(unlikely(eval.room_internal && !my(event)))
		throw error
		{
			fault::GENERAL, "Internal room event denied from external source."
		};

	// Wait for any pending duplicate evals before proceeding.
	assert(eval::count(event_id));
	if(likely(opts.phase[phase::DUPCHK] && opts.unique))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::DUPCHK
		};

		sequence::dock.wait([&event_id]
		{
			return eval::count(event_id) <= 1;
		});
	}

	if(unlikely(opts.unique && eval::count(event_id) > 1))
		throw error
		{
			fault::EXISTS, "Event is already being evaluated."
		};

	if(!opts.replays && !eval.copts && m::exists(event_id))
		throw error
		{
			fault::EXISTS, "Event has already been evaluated."
		};

	if(likely(opts.phase[phase::ACCESS]))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::ACCESS
		};

		call_hook(access_hook, eval, event, eval);
	}

	if(likely(opts.phase[phase::VERIFY]))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::VERIFY
		};

		if(!verify(event))
			throw m::BAD_SIGNATURE
			{
				"Signature verification failed."
			};
	}

	if(likely(opts.phase[phase::FETCH_AUTH] && opts.fetch))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::FETCH_AUTH
		};

		call_hook(fetch_auth_hook, eval, event, eval);
	}

	// Evaluation by auth system; throws
	if(likely(opts.phase[phase::AUTH_STATIC]) && authenticate)
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::AUTH_STATIC
		};

		const auto &[pass, fail]
		{
			room::auth::check_static(event)
		};

		if(!pass)
			throw error
			{
				fault::AUTH, "Fails against provided auth_events :%s",
				what(fail)
			};
	}

	if(likely(opts.phase[phase::FETCH_PREV] && opts.fetch))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::FETCH_PREV
		};

		call_hook(fetch_prev_hook, eval, event, eval);
	}

	if(likely(opts.phase[phase::FETCH_STATE] && opts.fetch))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::FETCH_STATE
		};

		call_hook(fetch_state_hook, eval, event, eval);
	}

	// Obtain sequence number here.
	const auto *const &top(eval::seqmax());
	eval.sequence =
	{
		top?
			std::max(sequence::get(*top) + 1, sequence::uncommitted + 1):
			sequence::committed + 1
	};

	log::debug
	{
		log, "%s event sequenced",
		loghead(eval)
	};

	const auto &parent_phase
	{
		eval.parent?
			eval.parent->phase:
			phase::NONE
	};

	const auto &parent_post
	{
		parent_phase == phase::POST
		&& eval.parent->event_
		&& eval.parent->event_->event_id
	};

	const scope_restore eval_phase_precommit
	{
		eval.phase, phase::PRECOMMIT
	};

	// Wait until this is the lowest sequence number
	sequence::dock.wait([&eval, &parent_post]
	{
		return false
		|| parent_post
		|| eval::seqnext(sequence::committed) == &eval
		|| eval::seqnext(sequence::uncommitted) == &eval
		;
	});

	if(likely(opts.phase[phase::AUTH_RELA] && authenticate))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::AUTH_RELA
		};

		const auto &[pass, fail]
		{
			room::auth::check_relative(event)
		};

		if(!pass)
			throw error
			{
				fault::AUTH, "Fails relative to the state at the event :%s",
				what(fail)
			};
	}

	assert(eval.sequence != 0);
	assert(eval::sequnique(sequence::get(eval)));
	//assert(sequence::uncommitted <= sequence::get(eval));
	//assert(sequence::committed < sequence::get(eval));
	assert(sequence::retired < sequence::get(eval));
	sequence::uncommitted = std::max(sequence::get(eval), sequence::uncommitted);

	const scope_restore eval_phase_commit
	{
		eval.phase, phase::COMMIT
	};

	// Wait until this is the lowest sequence number
	sequence::dock.wait([&eval, &parent_post]
	{
		return false
		|| parent_post
		|| eval::seqnext(sequence::committed) == &eval
		;
	});

	// Reevaluation of auth against the present state of the room.
	if(likely(opts.phase[phase::AUTH_PRES] && authenticate))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::AUTH_PRES
		};

		room::auth::check_present(event);
	}

	// Evaluation by module hooks
	if(likely(opts.phase[phase::EVALUATE]))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::EVALUATE
		};

		call_hook(eval_hook, eval, event, eval);
	}

	// Allocate transaction; discover shared-sequenced evals.
	if(likely(opts.phase[phase::INDEX]))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::INDEX
		};

		if(eval.txn)
			eval.txn->clear();

		if(!eval.txn && parent_post)
			eval.txn = eval.parent->txn;

		if(!eval.txn)
			eval.txn = std::make_shared<db::txn>
			(
				*dbs::events, db::txn::opts
				{
					calc_txn_reserve(opts, event),   // reserve_bytes
					0,                               // max_bytes (no max)
				}
			);

		// Transaction composition.
		write_append(eval, event);
	}

	// Generate post-eval/pre-notify effects. This function may conduct
	// an entire eval of several more events recursively before returning.
	if(likely(opts.phase[phase::POST]))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::POST
		};

		call_hook(post_hook, eval, event, eval);
	}

	assert(sequence::committed < sequence::get(eval));
	assert(sequence::retired < sequence::get(eval));
	sequence::committed = !parent_post?
		sequence::get(eval):
		sequence::committed;

	// Commit the transaction to database iff this eval is at the stack base.
	if(likely(opts.phase[phase::WRITE] && !parent_post))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::WRITE
		};

		write_commit(eval);
	}

	// Wait for sequencing only if this is the stack base, otherwise we'll
	// never return back to that stack base.
	if(likely(!parent_post))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::RETIRE
		};

		sequence::dock.wait([&eval]
		{
			return eval::seqnext(sequence::retired) == std::addressof(eval);
		});

		const auto next
		{
			eval::seqnext(eval.sequence)
		};

		const auto highest
		{
			next?
				sequence::get(*next):
				sequence::get(eval)
		};

		const auto retire
		{
			std::clamp
			(
				sequence::get(eval),
				sequence::retired + 1,
				highest
			)
		};

		log::debug
		{
			log, "%s %lu:%lu release %lu",
			loghead(eval),
			sequence::get(eval),
			retire,
			highest,
		};

		assert(sequence::get(eval) <= retire);
		assert(sequence::retired < sequence::get(eval));
		assert(sequence::retired < retire);
		sequence::retired = retire;
	}

	return fault::ACCEPT;
}

void
ircd::m::vm::write_append(eval &eval,
                          const event &event)

{
	assert(eval.opts);
	const auto &opts
	{
		*eval.opts
	};

	assert(eval.txn);
	auto &txn
	{
		*eval.txn
	};

	m::dbs::write_opts wopts(opts.wopts);
	wopts.interpose = eval.txn.get();
	wopts.event_idx = eval.sequence;
	wopts.json_source = true;

	// Don't update or resolve the room head with this shit.
	const bool dummy_event
	{
		json::get<"type"_>(event) == "org.matrix.dummy_event"
	};

	wopts.appendix.set
	(
		dbs::appendix::ROOM_HEAD,
		wopts.appendix[dbs::appendix::ROOM_HEAD] && !dummy_event
	);

	if(opts.present && json::get<"state_key"_>(event))
	{
		const room room
		{
			at<"room_id"_>(event)
		};

		const room::state state
		{
			room
		};

		//XXX
		const auto pres_idx
		{
			state.get(std::nothrow, at<"type"_>(event), at<"state_key"_>(event))
		};

		//XXX
		int64_t pres_depth(0);
		const auto fresher
		{
			!pres_idx ||
			m::get<int64_t>(pres_idx, "depth", pres_depth) < json::get<"depth"_>(event)
		};

		if(fresher)
		{
			//XXX
			const auto &[pass, fail]
			{
				opts.phase[phase::AUTH_PRES] && !eval.room_internal?
					room::auth::check_present(event):
					room::auth::passfail{true, {}}
			};

			if(fail)
				log::dwarning
				{
					log, "%s fails auth for present state of %s :%s",
					loghead(eval),
					string_view{room.room_id},
					what(fail),
				};

			wopts.appendix.set
			(
				dbs::appendix::ROOM_STATE,
				pass && wopts.appendix[dbs::appendix::ROOM_STATE]
			);

			wopts.appendix.set
			(
				dbs::appendix::ROOM_JOINED,
				pass && wopts.appendix[dbs::appendix::ROOM_JOINED]
			);
		}
	}

	dbs::write(*eval.txn, event, wopts);

	log::debug
	{
		log, "%s composed transaction",
		loghead(eval),
	};
}

void
ircd::m::vm::write_commit(eval &eval)
{
	assert(eval.txn);
	assert(eval.txn.use_count() == 1);
	auto &txn
	{
		*eval.txn
	};

	#ifdef RB_DEBUG
	const auto db_seq_before(db::sequence(*m::dbs::events));
	#endif

	txn();

	#ifdef RB_DEBUG
	const auto db_seq_after(db::sequence(*m::dbs::events));

	log::debug
	{
		log, "%s wrote %lu | db seq %lu:%lu %zu cells in %zu bytes to events database ...",
		loghead(eval),
		sequence::get(eval),
		db_seq_before,
		db_seq_after,
		txn.size(),
		txn.bytes()
	};
	#endif
}

size_t
ircd::m::vm::calc_txn_reserve(const opts &opts,
                              const event &event)
{
	const size_t reserve_event
	{
		opts.reserve_bytes == size_t(-1)?
			size_t(json::serialized(event) * 1.66):
			opts.reserve_bytes
	};

	return reserve_event + opts.reserve_index;
}

template<class T>
void
ircd::m::vm::call_hook(hook::site<T> &hook,
                       eval &eval,
                       const event &event,
                       T&& data)
try
{
	#if 0
	log::debug
	{
		log, "%s hook:%s enter",
		loghead(eval),
		hook.name(),
	};
	#endif

	// Providing a pointer to the eval.hook pointer allows the hook site to
	// provide updates for observers in other contexts for which hook is
	// currently entered.
	auto **const cur
	{
		std::addressof(eval.hook)
	};

	hook(cur, event, std::forward<T>(data));

	#if 0
	log::debug
	{
		log, "%s hook:%s leave",
		loghead(eval),
		hook.name(),
	};
	#endif
}
catch(const m::error &e)
{
	log::derror
	{
		log, "%s hook:%s :%s :%s",
		loghead(eval),
		hook.name(),
		e.errcode(),
		e.errstr(),
	};

	throw;
}
catch(const http::error &e)
{
	log::derror
	{
		log, "%s hook:%s :%s :%s",
		loghead(eval),
		hook.name(),
		e.what(),
		e.content,
	};

	throw;
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "%s hook:%s :%s",
		loghead(eval),
		hook.name(),
		e.what(),
	};

	throw;
}

template<class... args>
ircd::m::vm::fault
ircd::m::vm::handle_error(const vm::opts &opts,
                          const vm::fault &code,
                          const string_view &fmt,
                          args&&... a)
{
	if(opts.errorlog & code)
		log::error
		{
			log, fmt, std::forward<args>(a)...
		};
	else if(~opts.warnlog & code)
		log::derror
		{
			log, fmt, std::forward<args>(a)...
		};

	if(opts.warnlog & code)
		log::warning
		{
			log, fmt, std::forward<args>(a)...
		};

	if(~opts.nothrows & code)
		throw error
		{
			code, fmt, std::forward<args>(a)...
		};

	return code;
}
