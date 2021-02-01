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
	template<class... args> static bool output(const vm::opts &, const vm::fault &, const string_view &event_id, const string_view &fmt, args&&...);
	template<class... args> static fault handle_fault(const opts &, const fault &, const string_view &event_id, const string_view &fmt, args&&...);
	template<class T> static void call_hook(hook::site<T> &, eval &, const event &, T&& data);
	static void retire(eval &, const event &);
	static void emption_check(eval &, const event &);
	static size_t calc_txn_reserve(const opts &, const event &);
	static void write_commit(eval &);
	static void write_append(eval &, const event &, const bool &);
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

	const scope_count executing
	{
		eval::executing
	};

	const scope_restore eval_phase
	{
		eval.phase, phase::EXECUTE
	};

	const bool prefetch_keys
	{
		opts.phase[phase::VERIFY]
		&& opts.mfetch_keys
		&& events.size() > 1
	};

	const size_t prefetched_keys
	{
		prefetch_keys?
			fetch_keys(eval): 0UL
	};

	const bool prefetch_refs
	{
		opts.phase[phase::PREINDEX]
		&& opts.mprefetch_refs
		&& events.size() > 1
	};

	const size_t prefetched_refs
	{
		prefetch_refs?
			vm::prefetch_refs(eval): 0UL
	};

	size_t accepted(0), existed(0), i, j, k;
	for(i = 0; i < events.size(); i += j)
	{
		id::event ids[64];
		for(j = 0; j < 64 && i + j < events.size() && eval.evaluated + j < opts.limit; ++j)
			ids[j] = events[i + j].event_id;

		// Bitset indicating which events already exist.
		const uint64_t existing
		{
			!opts.replays?
				m::exists(vector_view<const id::event>(ids, j)):
				0UL
		};

		for(k = 0; k < j; ++k, ++eval.evaluated) try
		{
			const bool exists
			(
				existing & (1 << k)
			);

			const auto &event
			{
				events[i + k]
			};

			const auto fault
			{
				!exists?
					execute(eval, event):
					fault::EXISTS
			};

			existed += exists;
			accepted += fault == fault::ACCEPT;
			eval.accepted += fault == fault::ACCEPT;
			eval.faulted += fault != fault::ACCEPT;

			// If handle_fault() was not previously called about this eval
			if(likely(fault == fault::ACCEPT || exists))
				handle_fault(opts, fault, event.event_id?: eval.event_id, string_view{});
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
		json::get<"type"_>(event) == "m.room.create"
		&& json::get<"sender"_>(event)
		&& m::myself(json::get<"sender"_>(event))
	};

	// Query for whether the room apropos is an internal room. Note that the
	// room_id at this point may not be canonical; however, internal rooms
	// do not and never will never use non-canonical room_id's.
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

	// Reset the conformity report before and after this event's eval.
	const scope_restore eval_report
	{
		eval.report, event::conforms{}
	};

	// Conformity checks only require the event data itself; note that some
	// local queries may still be made by the hook, such as m::redacted().
	if(likely(opts.phase[phase::CONFORM]) && !opts.edu)
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::CONFORM
		};

		call_hook(conform_hook, eval, event, eval);
	}

	// If the event is simply missing content while not being authoritatively
	// redacted, the conformity phase would have thrown a prior exception. Now
	// we know if the event is legitimately missing content.
	const bool redacted
	{
		eval.report.has(event::conforms::MISMATCH_HASHES)
	};

	// If the input JSON is insufficient we'll need a buffer to rewrite the
	// event. This buffer can be reused by subsequent events in the eval.
	assert(!eval.buf || size(eval.buf) >= event::MAX_SIZE);
	if(!opts.edu && !eval.buf && (!opts.json_source || redacted))
		eval.buf = unique_mutable_buffer
		{
			event::MAX_SIZE, simd::alignment
		};

	// Conjure a view of the correct canonical JSON representation. This will
	// either reference the input directly or the rewrite into eval.buf.
	const json::object event_source
	{
		// Canonize from some other serialized source.
		likely(!opts.edu && !opts.json_source && event.source && !redacted)?
			json::stringify(mutable_buffer{eval.buf}, event.source):

		// Canonize from no source; usually taken when my(event).
		// XXX elision conditions go here
		likely(!opts.edu && !opts.json_source && !redacted)?
			json::stringify(mutable_buffer{eval.buf}, event):

		// Canonize and redact from some other serialized source.
		!opts.edu && !opts.json_source && event.source?
			json::stringify(mutable_buffer{eval.buf}, m::essential(event.source, event::buf[0], true)):

		// Canonize and redact from no source.
		!opts.edu && !opts.json_source?
			json::stringify(mutable_buffer{eval.buf}, m::essential(event, event::buf[0], true)):

		// Use the input directly.
		string_view{event.source}
	};

	// Create a new event tuple from the canonical source, otherwise reference
	// the existing input tuple directly. From now on we'll be referencing
	// `_event` instead of `event` to ensure we have canonical values.
	const m::event &_event
	{
		event_source?
			m::event{event_source}:
			event
	};

	// Now conjure the corrected room_id and reference that for the duration
	// of this event's eval.
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

	// Enter the phase to check and hold for other evals with the same event_id
	// to prevent race-conditions. Note that duplicates are blocked but never
	// rejected here, as the first eval might fail and the second might not.
	if(likely(opts.phase[phase::DUPWAIT]) && eval.event_id)
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::DUPWAIT
		};

		// Prevent more than one event with the same event_id from
		// being evaluated at the same time.
		if(likely(opts.unique))
			vm::dock.wait([&eval]
			{
				assert(eval::count(eval.event_id) <= 1);
				return eval::count(eval.event_id) == 0;
			});
	}

	// Point to the new event_id.
	const scope_restore event_event_id
	{
		mutable_cast(_event).event_id,
		event::id{eval.event_id}
	};

	// Set a member pointer to the event currently being evaluated. This
	// allows other parallel evals to have deep access to this eval. It also
	// will be used to count this event as currently being evaluated.
	assert(!eval.event_);
	const scope_restore eval_event
	{
		eval.event_, &_event
	};

	// Now that the final input is constructed and everything is known about it,
	// the next frame's exception handlers will build and propagate much better
	// error messages.
	return execute_du(eval, _event);
}
catch(const vm::error &e)
{
	throw; // propagate from execute_du
}
catch(const m::error &e)
{
	const ctx::exception_handler eh;
	const json::object &content
	{
		e.content
	};

	assert(eval.opts);
	return handle_fault
	(
		*eval.opts, fault::GENERAL, event.event_id,
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
	const ctx::exception_handler eh;

	assert(eval.opts);
	return handle_fault
	(
		*eval.opts, fault::GENERAL, event.event_id,
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
		event.event_id && !opts.edu?
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
	const ctx::exception_handler eh;
	const json::object &content{e.content};
	const json::string &error
	{
		content["error"]
	};

	assert(eval.opts);
	return handle_fault
	(
		*eval.opts, e.code, event.event_id,
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
	const ctx::exception_handler eh;
	const json::object &content{e.content};
	const json::string error[]
	{
		content["errcode"],
		content["error"]
	};

	assert(eval.opts);
	return handle_fault
	(
		*eval.opts, fault::GENERAL, event.event_id,
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
	const ctx::exception_handler eh;

	assert(eval.opts);
	return handle_fault
	(
		*eval.opts, fault::GENERAL, event.event_id,
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

	// There is no reason for an event from another origin to be sent to an
	// internal room. This boxes internal room access as a local problem, with
	// local mistakes.
	if(unlikely(eval.room_internal && !my(event)))
		throw error
		{
			fault::GENERAL, "Internal room event denied from external source."
		};

	// Check if an event with the same ID was already accepted.
	if(likely(opts.phase[phase::DUPCHK]))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::DUPCHK
		};

		// Prevent the same event from being accepted twice.
		if(likely(!opts.replays) && m::exists(event_id))
		{
			if(unlikely(~opts.nothrows & fault::EXISTS))
				throw error
				{
					fault::EXISTS, "Event has already been evaluated."
				};

			return fault::EXISTS;
		}
	}

	assert(!opts.unique || eval::count(event_id) == 1);
	assert(opts.replays || !m::exists(event_id));

	// Check if event's proprietor is denied by the room ACL.
	if(likely(opts.phase[phase::ACCESS]))
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::ACCESS
		};

		call_hook(access_hook, eval, event, eval);
	}

	// Check if this event is relevant to this server.
	if(likely(opts.phase[phase::EMPTION]) && !eval.room_internal)
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::EMPTION
		};

		emption_check(eval, event);
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
	const auto top(eval::seqmax());
	eval.sequence =
	{
		top?
			std::max(sequence::get(*top) + 1, sequence::uncommitted + 1):
			sequence::uncommitted + 1
	};

	log::debug
	{
		log, "%s event sequenced",
		loghead(eval)
	};

	assert(eval::sequnique(eval.sequence));
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

	// Allocate transaction; prefetch dependencies.
	if(likely(opts.phase[phase::PREINDEX]) && !opts.mprefetch_refs)
	{
		const scope_restore eval_phase
		{
			eval.phase, phase::PREINDEX
		};

		dbs::write_opts wopts(opts.wopts);
		wopts.event_idx = eval.sequence;
		const size_t prefetched
		{
			dbs::prefetch(event, wopts)
		};
	}

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

		// Transaction composition.
		write_append(eval, event, parent_post);
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

		retire(eval, event);
	}

	return fault::ACCEPT;
}

void
ircd::m::vm::retire(eval &eval,
                    const event &event)

{
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

namespace ircd::m::vm
{
	[[gnu::visibility("internal")]]
	extern stats::item<uint64_t>
	write_commit_count,
	write_commit_cycles;
}

decltype(ircd::m::vm::write_commit_cycles)
ircd::m::vm::write_commit_cycles
{
	{ "name", "ircd.m.vm.write_commit.cycles" },
};

decltype(ircd::m::vm::write_commit_count)
ircd::m::vm::write_commit_count
{
	{ "name", "ircd.m.vm.write_commit.count" },
};

void
ircd::m::vm::write_commit(eval &eval)
{
	assert(eval.txn);
	assert(eval.txn.use_count() == 1);
	auto &txn
	{
		*eval.txn
	};

	const auto db_seq_before
	{
		#ifdef RB_DEBUG
			db::sequence(*m::dbs::events)
		#else
			0UL
		#endif
	};

	const uint64_t cyc_before {write_commit_cycles};
	{
		const prof::scope_cycles cycles
		{
			write_commit_cycles
		};

		txn();
	}

	++write_commit_count;
	const auto db_seq_after
	{
		#ifdef RB_DEBUG
			db::sequence(*m::dbs::events)
		#else
			0UL
		#endif
	};

	log::debug
	{
		log, "%s wrote %lu | db seq:%lu:%lu txn:%lu cells:%zu in bytes:%zu cycles:%lu to events database",
		loghead(eval),
		sequence::get(eval),
		db_seq_before,
		db_seq_after,
		uint64_t(write_commit_count),
		txn.size(),
		txn.bytes(),
		uint64_t(write_commit_cycles) - cyc_before,
	};
}

void
ircd::m::vm::write_append(eval &eval,
                          const event &event,
                          const bool &parent_post)

{
	assert(eval.opts);
	const auto &opts
	{
		*eval.opts
	};

	assert(eval.room_id);
	const room room
	{
		eval.room_id
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

	const auto state_candidate
	{
		opts.present
		&& json::get<"state_key"_>(event)
	};

	const auto state_idx
	{
		state_candidate?
			room.get(std::nothrow, at<"type"_>(event), at<"state_key"_>(event)):
			0UL
	};

	const int64_t state_depth
	{
		m::get<int64_t>(std::nothrow, state_idx, "depth", 0L)
	};

	const auto state_present
	{
		!state_depth || state_depth < json::get<"depth"_>(event)
	};

	const bool authenticate
	{
		opts.auth
		&& opts.phase[phase::AUTH_PRES]
		&& state_present
		&& !eval.room_internal
	};

	const auto &[pass, fail]
	{
		authenticate?
			room::auth::check_present(event):
			room::auth::passfail{true, {}}
	};

	if(state_present && fail)
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
		wopts.appendix[dbs::appendix::ROOM_STATE] && state_present && pass
	);

	wopts.appendix.set
	(
		dbs::appendix::ROOM_JOINED,
		wopts.appendix[dbs::appendix::ROOM_JOINED] && state_present && pass
	);

	const size_t wrote
	{
		dbs::write(txn, event, wopts)
	};

	log::debug
	{
		log, "%s composed transaction wrote:%zu state:%b pres:%b prev:%lu @%ld",
		loghead(eval),
		wrote,
		state_candidate,
		state_present,
		state_idx,
		state_depth,
	};
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

void
ircd::m::vm::emption_check(eval &eval,
                           const m::event &event)
{
	const bool my_target_member_event
	{
		json::get<"type"_>(event) == "m.room.member"
		&& my(m::user(json::get<"state_key"_>(event)))
	};

	const bool allow
	{
		my(event)
		|| my_target_member_event
		|| m::local_joined(room::id(json::get<"room_id"_>(event)))
	};

	if(unlikely(!allow))
		throw vm::error
		{
			http::UNAUTHORIZED, fault::BOUNCE,
			"No users require events of type=%s%s%s in %s on this server.",
			json::get<"type"_>(event),
			json::get<"state_key"_>(event)?
				",state_key="_sv:
				string_view{},
			json::get<"state_key"_>(event),
			json::get<"room_id"_>(event),
		};
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
ircd::m::vm::handle_fault(const vm::opts &opts,
                          const vm::fault &code,
                          const string_view &event_id,
                          const string_view &fmt,
                          args&&... a)
{
	if(code != fault::ACCEPT && fmt)
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
	}

	if(likely(opts.outlog & code))
		output(opts, code, event_id, fmt, std::forward<args>(a)...);

	if(code != fault::ACCEPT && fmt)
		if(unlikely(~opts.nothrows & code))
			throw error
			{
				code, fmt, std::forward<args>(a)...
			};

	return code;
}

//NOTE: may yield on a json::stack flush
template<class... args>
bool
ircd::m::vm::output(const vm::opts &opts,
                    const vm::fault &code,
                    const string_view &event_id,
                    const string_view &fmt,
                    args&&... a)
{
	if(!opts.out)
		return false;

	if(unlikely(!event_id))
		return false;

	json::stack::object object
	{
		*opts.out, event_id
	};

	if(code != fault::ACCEPT)
		json::stack::member
		{
			object, "errcode", json::value
			{
				reflect(code), json::STRING
			}
		};

	if(!fmt)
		return true;

	const fmt::bsprintf<1024> text
	{
		fmt, std::forward<args>(a)...
	};

	json::stack::member
	{
		object, "error", json::value
		{
			string_view{text}, json::STRING
		}
	};

	return true;
}
