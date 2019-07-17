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
	static void write_prepare(eval &, const event &);
	static fault execute_edu(eval &, const event &);
	static fault execute_pdu(eval &, const event &);
	fault execute(eval &, const event &);
	static fault inject3(eval &, json::iov &, const json::iov &);
	static fault inject1(eval &, json::iov &, const json::iov &);
	fault inject(eval &, json::iov &, const json::iov &);
	static void fini();
	static void init();

	extern hook::site<eval &> issue_hook;    ///< Called when this server is issuing event
	extern hook::site<eval &> conform_hook;  ///< Called for static evaluations of event
	extern hook::site<eval &> access_hook;   ///< Called for access control checking
	extern hook::site<eval &> fetch_hook;    ///< Called to resolve dependencies
	extern hook::site<eval &> eval_hook;     ///< Called for final event evaluation
	extern hook::site<eval &> post_hook;     ///< Called to apply effects pre-notify
	extern hook::site<eval &> notify_hook;   ///< Called to broadcast successful eval
	extern hook::site<eval &> effect_hook;   ///< Called to apply effects post-notify

	extern conf::item<bool> log_commit_debug;
	extern conf::item<bool> log_accept_debug;
	extern conf::item<bool> log_accept_info;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix Virtual Machine",
	ircd::m::vm::init, ircd::m::vm::fini
};

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
	{ "default",  false                        },
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

decltype(ircd::m::vm::fetch_hook)
ircd::m::vm::fetch_hook
{
	{ "name", "vm.fetch" }
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
	{ "name", "vm.notify" }
};

decltype(ircd::m::vm::effect_hook)
ircd::m::vm::effect_hook
{
	{ "name", "vm.effect" }
};

//
// init
//

void
ircd::m::vm::init()
{
	id::event::buf event_id;
	sequence::retired = sequence::get(event_id);
	sequence::committed = sequence::retired;
	sequence::uncommitted = sequence::committed;

	vm::ready = true;
	vm::dock.notify_all();

	log::info
	{
		log, "BOOT %s @%lu [%s]",
		string_view{m::my_node.node_id},
		sequence::retired,
		sequence::retired? string_view{event_id} : "NO EVENTS"_sv
	};
}

void
ircd::m::vm::fini()
{
	vm::ready = false;

	if(!eval::list.empty())
		log::warning
		{
			log, "Waiting for %zu evals (exec:%zu inject:%zu pending:%zu)",
			eval::list.size(),
			eval::executing,
			eval::injecting,
			sequence::pending,
		};

	vm::dock.wait([]
	{
		return !eval::executing && !eval::injecting;
	});

	assert(!sequence::pending);

	event::id::buf event_id;
	const auto retired
	{
		sequence::get(event_id)
	};

	log::info
	{
		log, "HLT '%s' @%lu [%s] %lu:%lu:%lu",
		string_view{m::my_node.node_id},
		retired,
		retired? string_view{event_id} : "NO EVENTS"_sv,
		sequence::retired,
		sequence::committed,
		sequence::uncommitted
	};

	assert(retired == sequence::retired);
}

//
// eval
//

enum ircd::m::vm::fault
IRCD_MODULE_EXPORT
ircd::m::vm::inject(eval &eval,
                    json::iov &event,
                    const json::iov &contents)
{
	// We need a copts structure in addition to the opts structure in order
	// to inject a new event. If one isn't supplied a default is referenced.
	eval.copts = !eval.copts?
		&vm::default_copts:
		eval.copts;

	// Note that the regular opts is unconditionally overridden because the
	// user should have provided copts instead.
	assert(!eval.opts || eval.opts == eval.copts);
	eval.opts = eval.copts;

	// copts inherits from opts; for the purpose of this frame we consider
	// the options structure to be all of it.
	assert(eval.opts);
	assert(eval.copts);
	const auto &opts
	{
		*eval.copts
	};

	// This semaphore gets unconditionally pinged when this scope ends.
	const scope_notify notify
	{
		vm::dock
	};

	// The count of contexts currently conducting an event injection is
	// incremented here and decremented at unwind.
	const scope_count eval_injecting
	{
		eval::injecting
	};

	// Set a member pointer to the json::iov currently being composed. This
	// allows other parallel evals to have deep access to exactly what this
	// eval is attempting to do.
	const scope_restore eval_issue
	{
		eval.issue, &event
	};

	// Common indicator which will determine if several branches are taken as
	// a room create event has several special cases.
	const bool is_room_create
	{
		event.at("type") == "m.room.create"
	};

	// The eval structure has a direct room::id reference for interface
	// convenience so people don't have to figure out what room (if any)
	// this injection is targeting. That reference might already be set
	// by the user as a hint; if not, we attempt to set it here and tie
	// it to the duration of this frame.
	const scope_restore eval_room_id
	{
		eval.room_id,
		!eval.room_id && event.has("room_id")?
			string_view{event.at("room_id")}:
			string_view{eval.room_id}
	};

	// Attempt to resolve the room version at this point for interface
	// exposure at vm::eval::room_version.
	char room_version_buf[32];
	const scope_restore eval_room_version
	{
		eval.room_version,

		// If the eval.room_version interface reference is already set to
		// something we assume the room_version has alreandy been resolved
		eval.room_version?
			eval.room_version:

		// If the options had a room_version set, consider that the room
		// version. The user has already resolved the room version and is
		// hinting us as an optimization.
		eval.opts->room_version?
			eval.opts->room_version:

		// If this is an m.room.create event then we're lucky that the best
		// room version information is in the spec location.
		is_room_create && contents.has("room_version")?
			string_view{contents.at("room_version")}:

		// Make a query to find the version. The version string will be hosted
		// by the stack buffer.
			m::version(room_version_buf, room{eval.room_id}, std::nothrow)
	};

	// Conditionally add the room_id from the eval structure to the actual
	// event iov being injected. This is the inverse of the above satisfying
	// the case where the room_id is supplied via the reference, not the iov;
	// in the end we want that reference in both places.
	assert(eval.room_id);
	const json::iov::add room_id_
	{
		event, eval.room_id && !event.has("room_id"),
		{
			"room_id", [&eval]() -> json::value
			{
				return eval.room_id;
			}
		}
	};

	// XXX: should move outside if lazy static initialization is problem.
	static conf::item<size_t> prev_limit
	{
		{ "name",         "ircd.m.vm.inject.prev.limit" },
		{ "default",      16L                           },
		{
			"description",
			"Events created by this server will only"
			" reference a maximum of this many prev_events."
		},
	};

	// Ad hoc number of bytes we'll need for each prev_events reference in
	// a v1 event. We don't use the hashes in prev_events, so we just need
	// space for one worst-case event_id and some JSON.
	static const size_t prev_scalar_v1
	{
		(id::MAX_SIZE + 1) * 2
	};

	// Ad hoc number of bytes we'll need for each prev_events reference in
	// a sha256-b64 event_id format.
	static const size_t prev_scalar_v3
	{
		// "   $   XX   "   ,
		   1 + 1 + 43 + 1 + 1 + 1
	};

	const auto &prev_scalar
	{
		eval.room_version == "1" || eval.room_version == "2"?
			prev_scalar_v1:
			prev_scalar_v3
	};

	// The buffer we'll be composing the prev_events JSON array into.
	const unique_buffer<mutable_buffer> prev_buf
	{
		!is_room_create && opts.add_prev_events?
			std::min(size_t(prev_limit) * prev_scalar, event::MAX_SIZE):
			0UL
	};

	// Conduct the prev_events composition into our buffer. This sub returns
	// a finished json::array in our buffer as well as a depth integer for
	// the event which will be using the references.
	const room::head head{room{eval.room_id}};
	const auto &[prev_events, depth]
	{
		!is_room_create && opts.add_prev_events?
			head.make_refs(prev_buf, size_t(prev_limit), true):
			std::pair<json::array, int64_t>{{}, -1}
	};

	// Add the prev_events
	const json::iov::add prev_events_
	{
		event, opts.add_prev_events && !empty(prev_events),
		{
			"prev_events", [&prev_events]() -> json::value
			{
				return prev_events;
			}
		}
	};

	// Conditionally add the depth property to the event iov.
	assert(depth >= -1);
	const json::iov::set depth_
	{
		event, opts.add_depth && !event.has("depth"),
		{
			"depth", [&depth]
			{
				// When the depth value is undefined_number it was intended
				// that no depth should appear in the event JSON so that value
				// is preserved; we also don't overflow the integer, so if the
				// depth is at max value that is preserved too.
				return
					depth == std::numeric_limits<int64_t>::max() ||
					depth == json::undefined_number?
						json::value{depth}:
						json::value{depth + 1};
			}
		}
	};

	// The auth_events have more deterministic properties.
	static const size_t auth_buf_sz{m::id::MAX_SIZE * 4};
	const unique_buffer<mutable_buffer> auth_buf
	{
		!is_room_create && opts.add_auth_events? auth_buf_sz : 0UL
	};

	// Default to an empty array.
	json::array auth_events
	{
		json::empty_array
	};

	// Conditionally compose the auth events.
	if(!is_room_create && opts.add_auth_events)
	{
		const room::auth auth{room{eval.room_id}};
		auth_events = auth.make_refs(auth_buf, m::event{event});
	}

	// Conditionally add the auth_events to the event iov.
	const json::iov::add auth_events_
	{
		event, opts.add_auth_events,
		{
			"auth_events", [&auth_events]() -> json::value
			{
				return auth_events;
			}
		}
	};

	// Add our network name.
	const json::iov::add origin_
	{
		event, opts.add_origin,
		{
			"origin", []() -> json::value
			{
				return my_host();
			}
		}
	};

	// Add the current time.
	const json::iov::add origin_server_ts_
	{
		event, opts.add_origin_server_ts,
		{
			"origin_server_ts", []
			{
				return json::value{ircd::time<milliseconds>()};
			}
		}
	};

	return eval.room_version == "1" || eval.room_version == "2"?
		inject1(eval, event, contents):
		inject3(eval, event, contents);
}

/// Old event branch
enum ircd::m::vm::fault
ircd::m::vm::inject1(eval &eval,
                     json::iov &event,
                     const json::iov &contents)
{
	assert(eval.copts);
	const auto &opts
	{
		*eval.copts
	};

	// event_id

	assert(eval.room_version);
	const event::id &event_id
	{
		opts.add_event_id?
			make_id(m::event{event}, eval.room_version, eval.event_id):
			event::id{}
	};

	const json::iov::add event_id_
	{
		event, !empty(event_id),
		{
			"event_id", [&event_id]() -> json::value
			{
				return event_id;
			}
		}
	};

	// Stringify the event content into buffer
	const json::strung content
	{
		contents
	};

	// hashes

	char hashes_buf[384];
	const string_view hashes
	{
		opts.add_hash?
			m::event::hashes(hashes_buf, event, content):
			string_view{}
	};

	const json::iov::add hashes_
	{
		event, opts.add_hash && !empty(hashes),
		{
			"hashes", [&hashes]() -> json::value
			{
				return hashes;
			}
		}
	};

	// sigs

	char sigs_buf[384];
	const string_view sigs
	{
		opts.add_sig?
			m::event::signatures(sigs_buf, event, contents):
			string_view{}
	};

	const json::iov::add sigs_
	{
		event, opts.add_sig,
		{
			"signatures", [&sigs]() -> json::value
			{
				return sigs;
			}
		}
	};

	const json::iov::push content_
	{
		event, { "content", content },
	};

	const m::event event_tuple
	{
		event, event_id
	};

	if(opts.debuglog_precommit)
		log::debug
		{
			log, "Issuing: %s", pretty_oneline(event_tuple)
		};

	return execute(eval, event_tuple);
}

/// New event branch
enum ircd::m::vm::fault
ircd::m::vm::inject3(eval &eval,
                     json::iov &event,
                     const json::iov &contents)
{
	assert(eval.copts);
	const auto &opts
	{
		*eval.copts
	};

	// Stringify the event content into buffer
	const json::strung content
	{
		contents
	};

	// Compute the content hash into buffer.
	char hashes_buf[384];
	const string_view hashes
	{
		opts.add_hash?
			m::event::hashes(hashes_buf, event, content):
			string_view{}
	};

	// Add the content hash to the event iov.
	const json::iov::add hashes_
	{
		event, opts.add_hash && !empty(hashes),
		{
			"hashes", [&hashes]() -> json::value
			{
				return hashes;
			}
		}
	};

	// Compute the signature into buffer.
	char sigs_buf[384];
	const string_view sigs
	{
		opts.add_sig?
			m::event::signatures(sigs_buf, event, contents):
			string_view{}
	};

	// Add the signature to the event iov.
	const json::iov::add sigs_
	{
		event, opts.add_sig,
		{
			"signatures", [&sigs]() -> json::value
			{
				return sigs;
			}
		}
	};

	// Add the content to the event iov
	const json::iov::push content_
	{
		event, { "content", content },
	};

	// Compute the event_id (reference hash) into the buffer
	// in the eval interface so it persists longer than this stack.
	const event::id &event_id
	{
		opts.add_event_id?
			make_id(m::event{event}, eval.room_version, eval.event_id):
			event::id{}
	};

	// Transform the json iov into a json tuple
	const m::event event_tuple
	{
		event, event_id
	};

	if(opts.debuglog_precommit)
		log::debug
		{
			log, "Issuing: %s", pretty_oneline(event_tuple)
		};

	return execute(eval, event_tuple);
}

enum ircd::m::vm::fault
IRCD_MODULE_EXPORT
ircd::m::vm::execute(eval &eval,
                     const event &event)
try
{
	// m::vm bookkeeping that someone entered this function
	const scope_count executing{eval::executing};
	const scope_notify notify{vm::dock};

	// Set a member pointer to the event currently being evaluated. This
	// allows other parallel evals to have deep access to this eval.
	assert(!eval.event_);
	const scope_restore eval_event
	{
		eval.event_, &event
	};

	// Set a member to the room_id for convenient access, without stepping on
	// any room_id reference that exists there for whatever reason.
	const scope_restore eval_room_id
	{
		eval.room_id,
		eval.room_id?
			eval.room_id:
			string_view(json::get<"room_id"_>(event))
	};

	// Procure the room version.
	char room_version_buf[32];
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

		// Make a query for the room version into the stack buffer.
		m::version(room_version_buf, room{eval.room_id}, std::nothrow)
	};

	assert(eval.opts);
	assert(eval.event_);
	assert(eval.id);
	assert(eval.ctx);
	const auto &opts
	{
		*eval.opts
	};

	// The issue hook is only called when this server is injecting a newly
	// created event.
	if(eval.copts && eval.copts->issue)
		call_hook(issue_hook, eval, event, eval);

	// The conform hook runs static checks on an event's formatting and
	// composure; these checks only require the event data itself.
	if(opts.conform)
	{
		const ctx::critical_assertion ca;
		call_hook(conform_hook, eval, event, eval);
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

	// The event was executed; now we broadcast the good news. This will
	// include notifying client `/sync` and the federation sender.
	if(opts.notify)
		call_hook(notify_hook, eval, event, eval);

	// The "effects" of the event are created by listeners on the effect hook.
	// These can include the creation of even more events, such as creating a
	// PDU out of an EDU, etc. Unlike the post_hook in execute_pdu(), the
	// notify for the event at issue here has already been made.
	if(opts.effects)
		call_hook(effect_hook, eval, event, eval);

	if(opts.debuglog_accept || bool(log_accept_debug))
		log::debug
		{
			log, "%s", pretty_oneline(event)
		};

	if(opts.infolog_accept || bool(log_accept_info))
		log::info
		{
			log, "%s", pretty_oneline(event)
		};

	return ret;
}
catch(const error &e) // VM FAULT CODE
{
	return handle_error
	(
		*eval.opts, e.code,
		"eval %s :%s",
		event.event_id? string_view{event.event_id}: "<edu>"_sv,
		unquote(json::object(e.content).get("error"))
	);
}
catch(const m::error &e) // GENERAL MATRIX ERROR
{
	return handle_error
	(
		*eval.opts, fault::GENERAL,
		"eval %s (General Protection) :%s :%s :%s",
		event.event_id? string_view{event.event_id}: "<edu>"_sv,
		e.what(),
		unquote(json::object(e.content).get("errcode")),
		unquote(json::object(e.content).get("error"))
	);
}
catch(const ctx::interrupted &e) // INTERRUPTION
{
	return handle_error
	(
		*eval.opts, fault::INTERRUPT,
		"eval %s :%s",
		event.event_id? string_view{event.event_id}: "<edu>"_sv,
		e.what()
	);
}
catch(const std::exception &e) // ALL OTHER ERRORS
{
	return handle_error
	(
		*eval.opts, fault::GENERAL,
		"eval %s (General Protection) :%s",
		event.event_id? string_view{event.event_id}: "<edu>"_sv,
		e.what()
	);
}

enum ircd::m::vm::fault
ircd::m::vm::execute_edu(eval &eval,
                         const event &event)
{
	if(eval.opts->eval)
		call_hook(eval_hook, eval, event, eval);

	if(eval.opts->post)
		call_hook(post_hook, eval, event, eval);

	return fault::ACCEPT;
}

enum ircd::m::vm::fault
ircd::m::vm::execute_pdu(eval &eval,
                         const event &event)
{
	const scope_count pending
	{
		sequence::pending
	};

	const scope_restore remove_txn
	{
		eval.txn, std::shared_ptr<db::txn>{}
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

	const bool already_exists
	{
		exists(event_id)
	};

	  //TODO: ABA
	if(already_exists && !opts.replays)
		throw error
		{
			fault::EXISTS, "Event has already been evaluated."
		};

	if(opts.access)
		call_hook(access_hook, eval, event, eval);

	if(opts.verify && !verify(event))
		throw m::BAD_SIGNATURE
		{
			"Signature verification failed"
		};

	// Fetch dependencies
	if(opts.fetch)
		call_hook(fetch_hook, eval, event, eval);

	// Obtain sequence number here.
	const auto *const &top(eval::seqmax());
	eval.sequence_shared[0] = 0;
	eval.sequence_shared[1] = 0;
	eval.sequence = 0;
	eval.sequence =
	{
		top?
			std::max(sequence::get(*top) + 1, sequence::committed + 1):
			sequence::committed + 1
	};

	log::debug
	{
		log, "%s | acquire", loghead(eval)
	};

	assert(eval.sequence != 0);
	assert(sequence::uncommitted <= sequence::get(eval));
	assert(sequence::committed < sequence::get(eval));
	assert(sequence::retired < sequence::get(eval));
	assert(eval::sequnique(sequence::get(eval)));
	sequence::uncommitted = sequence::get(eval);

	// Evaluation by module hooks
	if(opts.eval)
		call_hook(eval_hook, eval, event, eval);

	// Wait until this is the lowest sequence number
	sequence::dock.wait([&eval]
	{
		return eval::seqnext(sequence::committed) == &eval;
	});

	log::debug
	{
		log, "%s | commit", loghead(eval)
	};

	assert(sequence::committed < sequence::get(eval));
	assert(sequence::retired < sequence::get(eval));
	sequence::committed = sequence::get(eval);
	sequence::dock.notify_all();

	if(opts.write)
		write_prepare(eval, event);

	if(opts.write)
		write_append(eval, event);

	// Generate post-eval/pre-notify effects. This function may conduct
	// an entire eval of several more events recursively before returning.
	if(opts.post)
		call_hook(post_hook, eval, event, eval);

	// Commit the transaction to database iff this eval is at the stack base.
	if(opts.write && !eval.sequence_shared[0])
		write_commit(eval);

	// Wait for sequencing only if this is the stack base, otherwise we'll
	// never return back to that stack base.
	if(!eval.sequence_shared[0])
	{
		sequence::dock.wait([&eval]
		{
			return eval::seqnext(sequence::retired) == &eval;
		});

		log::debug
		{
			log, "%s | retire %lu:%lu",
			loghead(eval),
			sequence::get(eval),
			eval.sequence_shared[1],
		};

		assert(sequence::retired < sequence::get(eval));
		sequence::retired = std::max(eval.sequence_shared[1], sequence::get(eval));
		sequence::dock.notify_all();
	}

	return fault::ACCEPT;
}

void
ircd::m::vm::write_prepare(eval &eval,
                           const event &event)

{
	assert(eval.opts);
	const auto &opts{*eval.opts};

	// Share a transaction with any other unretired evals on this stack. This
	// should mean the bottom-most/lowest-sequence eval on this ctx.
	const auto get_other_txn{[&eval]
	(auto &other)
	{
		if(&other == &eval)
			return true;

		if(!other.txn)
			return true;

		if(sequence::get(other) <= sequence::retired)
			return true;

		other.sequence_shared[1] = std::max(other.sequence_shared[1], sequence::get(eval));
		eval.sequence_shared[0] = sequence::get(other);
		eval.txn = other.txn;
		return false;
	}};

	// If we broke from the iteration then this eval is sharing a transaction
	// from another eval on this stack.
	if(!eval.for_each(eval.ctx, get_other_txn))
		return;

	eval.txn = std::make_shared<db::txn>
	(
		*dbs::events, db::txn::opts
		{
			calc_txn_reserve(opts, event),   // reserve_bytes
			0,                               // max_bytes (no max)
		}
	);
}

void
ircd::m::vm::write_append(eval &eval,
                          const event &event)

{
	assert(eval.opts);
	assert(eval.txn);

	const auto &opts{*eval.opts};
	auto &txn{*eval.txn};

	log::debug
	{
		log, "%s | append", loghead(eval)
	};

	// Preliminary write_opts
	m::dbs::write_opts wopts(opts.wopts);
	wopts.appendix.set(dbs::appendix::ROOM_STATE, opts.present);
	wopts.appendix.set(dbs::appendix::ROOM_JOINED, opts.present);
	wopts.appendix.set(dbs::appendix::ROOM_STATE_SPACE, opts.history);
	wopts.appendix.set(dbs::appendix::ROOM_HEAD, opts.room_head);
	wopts.appendix.set(dbs::appendix::ROOM_HEAD_RESOLVE, opts.room_head_resolve);
	wopts.json_source = opts.json_source;
	wopts.event_idx = eval.sequence;
	dbs::write(*eval.txn, event, wopts);
}

void
ircd::m::vm::write_commit(eval &eval)
{
	assert(eval.txn);
	assert(eval.txn.use_count() == 1);
	assert(eval.sequence_shared[0] == 0);
	auto &txn(*eval.txn);

	#ifdef RB_DEBUG
	const auto db_seq_before(db::sequence(*m::dbs::events));
	#endif

	txn();

	#ifdef RB_DEBUG
	const auto db_seq_after(db::sequence(*m::dbs::events));

	log::debug
	{
		log, "%s | wrote  %lu:%lu | db seq %lu:%lu %zu cells in %zu bytes to events database ...",
		loghead(eval),
		sequence::get(eval),
		eval.sequence_shared[1],
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
	hook(event, std::forward<T>(data));
}
catch(const m::error &e)
{
	log::derror
	{
		"%s | phase:%s :%s :%s :%s",
		loghead(eval),
		unquote(hook.feature.get("name")),
		e.what(),
		e.errcode(),
		e.errstr(),
	};

	throw;
}
catch(const http::error &e)
{
	log::derror
	{
		"%s | phase:%s :%s :%s",
		loghead(eval),
		unquote(hook.feature.get("name")),
		e.what(),
		e.content,
	};

	throw;
}
catch(const std::exception &e)
{
	log::derror
	{
		"%s | phase:%s :%s",
		loghead(eval),
		unquote(hook.feature.get("name")),
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
