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
	static fault inject3(eval &, json::iov &, const json::iov &);
	static fault inject1(eval &, json::iov &, const json::iov &);
}

///
/// Figure 1:
///          in     .  <-- injection
///    ===:::::::==//
///    |  ||||||| //   <-- these functions
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

ircd::m::vm::fault
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
		!eval.room_id && event.has("room_id") && valid(id::ROOM, event.at("room_id"))?
			string_view{event.at("room_id")}:
			string_view{eval.room_id}
	};

	const scope_restore eval_room_internal
	{
		eval.room_internal,
		eval.room_internal?
			eval.room_internal:
		eval.room_id && my(room::id(eval.room_id))?
			m::internal(eval.room_id):
			false
	};

	// Attempt to resolve the room version at this point for interface
	// exposure at vm::eval::room_version.
	char room_version_buf[room::VERSION_MAX_SIZE];
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

		// If this is an EDU or some kind of feature without a room_id then
		// we'll leave this blank.
		!eval.room_id?
			string_view{}:

		// Make a query to find the version. The version string will be hosted
		// by the stack buffer.
			m::version(room_version_buf, room{eval.room_id}, std::nothrow)
	};

	// Conditionally add the room_id from the eval structure to the actual
	// event iov being injected. This is the inverse of the above satisfying
	// the case where the room_id is supplied via the reference, not the iov;
	// in the end we want that reference in both places.
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

	const bool add_prev_events
	{
		!is_room_create
		&& opts.prop_mask.has("prev_events")
		&& !event.has("prev_events")
		&& eval.room_id
	};

	// The buffer we'll be composing the prev_events JSON array into.
	const unique_buffer<mutable_buffer> prev_buf
	{
		add_prev_events?
			std::min(size_t(prev_limit) * (prev_scalar + 1), event::MAX_SIZE):
			0UL
	};

	// Conduct the prev_events composition into our buffer. This sub returns
	// a finished json::array in our buffer as well as a depth integer for
	// the event which will be using the references.
	const room::head head
	{
		add_prev_events?
			room::head{room{eval.room_id}}:
			room::head{}
	};

	const room::head::generate prev_events
	{
		prev_buf, head,
		{
			16,                    // .limit = 16,
			true, // !eval.room_internal,   // .need_top_head = true for non-internal rooms
			!eval.room_internal,   // .need_my_head = true for non-internal rooms
			eval.room_version      // .version = eval.room_version,
		}
	};

	// Add the prev_events
	const json::iov::add prev_events_
	{
		event, add_prev_events && !empty(prev_events.array),
		{
			"prev_events", [&prev_events]() -> json::value
			{
				return prev_events.array;
			}
		}
	};

	const auto &depth
	{
		prev_events.depth[1]
	};

	// Conditionally add the depth property to the event iov.
	const json::iov::set depth_
	{
		event, opts.prop_mask.has("depth") && !event.has("depth"),
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
					depth >= -1?
						json::value{depth + 1}:
						json::value{json::undefined_number};
			}
		}
	};

	const bool add_auth_events
	{
		!is_room_create
		&& opts.prop_mask.has("auth_events")
		&& !event.has("auth_events")
		&& eval.room_id
	};

	// The auth_events have more deterministic properties.
	static const size_t auth_buf_sz{m::id::MAX_SIZE * 4};
	const unique_buffer<mutable_buffer> auth_buf
	{
		add_auth_events? auth_buf_sz : 0UL
	};

	// Conditionally compose the auth events. efault to an empty array.
	const json::array auth_events
	{
		add_auth_events?
			room::auth::generate(auth_buf, m::room{eval.room_id}, m::event{event}):
			json::empty_array
	};

	// Conditionally add the auth_events to the event iov.
	const json::iov::add auth_events_
	{
		event, add_auth_events,
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
		event, opts.prop_mask.has("origin"),
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
		event, opts.prop_mask.has("origin_server_ts"),
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
ircd::m::vm::fault
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
		opts.prop_mask.has("event_id")?
			eval.event_id.assigned(make_id(m::event{event}, eval.room_version, eval.event_id)):
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
		opts.prop_mask.has("hashes")?
			m::event::hashes(hashes_buf, event, content):
			string_view{}
	};

	const json::iov::add hashes_
	{
		event, opts.prop_mask.has("hashes") && !empty(hashes),
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
		opts.prop_mask.has("signatures")?
			m::event::signatures(sigs_buf, event, contents):
			string_view{}
	};

	const json::iov::add sigs_
	{
		event, opts.prop_mask.has("signatures"),
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

	const vector_view events
	{
		&event_tuple, 1
	};

	return execute(eval, events);
}

/// New event branch
ircd::m::vm::fault
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
		opts.prop_mask.has("hashes")?
			m::event::hashes(hashes_buf, event, content):
			string_view{}
	};

	// Add the content hash to the event iov.
	const json::iov::add hashes_
	{
		event, opts.prop_mask.has("hashes") && !empty(hashes),
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
		opts.prop_mask.has("signatures")?
			m::event::signatures(sigs_buf, event, contents):
			string_view{}
	};

	// Add the signature to the event iov.
	const json::iov::add sigs_
	{
		event, opts.prop_mask.has("signatures"),
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
		opts.prop_mask.has("event_id")?
			eval.event_id.assigned(make_id(m::event{event}, eval.room_version, eval.event_id)):
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

	const vector_view events
	{
		&event_tuple, 1
	};

	return execute(eval, events);
}
