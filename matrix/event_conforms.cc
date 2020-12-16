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
	extern m::hookfn<eval &> conform_check_event_id;
	extern m::hookfn<eval &> conform_check_origin;
	extern m::hookfn<eval &> conform_check_size;
	extern m::hookfn<eval &> conform_report;
}

/// Check if event_id is sufficient for the room version.
decltype(ircd::m::vm::conform_check_event_id)
ircd::m::vm::conform_check_event_id
{
	{
		{ "_site", "vm.conform" }
	},
	[](const m::event &event, eval &eval)
	{
		// Don't care about EDU's on this hook
		if(!event.event_id)
			return;

		// Conditions for when we don't care if the event_id conforms. This
		// hook only cares if the event_id is sufficient for the version, and
		// we don't care about the early matrix versions with mxids here.
		const bool unaffected
		{
			!eval.room_version
			|| eval.room_version == "0"
			|| eval.room_version == "1"
			|| eval.room_version == "2"
		};

		if(eval.room_version == "3")
			if(!event::id::v3::is(event.event_id))
				throw error
				{
					fault::INVALID, "Event ID %s is not sufficient for version 3 room.",
					string_view{event.event_id}
				};

		// note: we check v4 format for all other room versions, including "4"
		if(!unaffected && eval.room_version != "3")
			if(!event::id::v4::is(event.event_id))
				throw error
				{
					fault::INVALID, "Event ID %s in a version %s room is not a version 4 Event ID.",
					string_view{event.event_id},
					eval.room_version,
				};
	}
};

/// Check if an eval with a copts structure (indicating this server is
/// creating the event) has an origin set to !my_host().
decltype(ircd::m::vm::conform_check_origin)
ircd::m::vm::conform_check_origin
{
	{
		{ "_site", "vm.conform" }
	},
	[](const m::event &event, eval &eval)
	{
		if(eval.opts && !eval.opts->conforming)
			return;

		if(unlikely(eval.copts && !my_host(at<"origin"_>(event))))
			throw error
			{
				fault::INVALID, "Issuing event for origin: %s", at<"origin"_>(event)
			};
	}
};

/// Check if an event originating from this server exceeds maximum size.
decltype(ircd::m::vm::conform_check_size)
ircd::m::vm::conform_check_size
{
	{
		{ "_site",  "vm.conform"  },
	},
	[](const m::event &event, eval &eval)
	{
		const size_t &event_size
		{
			serialized(event)
		};

		if(event_size > size_t(event::max_size))
			throw m::BAD_JSON
			{
				"Event is %zu bytes which is larger than the maximum %zu bytes",
				event_size,
				size_t(event::max_size)
			};
	}
};

/// Generate the conformity report and place the result into the eval. This
/// hook may do some IO to find out if an event is the target of a redaction.
decltype(ircd::m::vm::conform_report)
ircd::m::vm::conform_report
{
	{
		{ "_site",  "vm.conform"  }
	},
	[](const m::event &event, eval &eval)
	{
		assert(eval.opts);
		const auto &opts(*eval.opts);

		// When opts.conformed is set the report is already generated
		if(opts.conformed)
		{
			eval.report = opts.report;
			return;
		}

		// Mask of checks to be bypassed
		auto non_conform
		{
			opts.non_conform
		};

		// This hook is called prior to event_id determination; must be skipped
		non_conform.set(event::conforms::INVALID_OR_MISSING_EVENT_ID);

		// For internal rooms for now.
		if(eval.room_internal)
			non_conform.set(event::conforms::MISMATCH_ORIGIN_SENDER);

		// Generate the report here.
		eval.report = event::conforms
		{
			event, non_conform.report
		};

		// When opts.conforming is false a bad report is not an error.
		if(!opts.conforming)
			return;

		const bool allow_redaction
		{
			// allowed by origin server
			eval.report.has(event::conforms::MISMATCH_HASHES)
			&& opts.require_content <= 0
			&& opts.node_id == json::get<"origin"_>(event)?
				true:

			// allowed by my server
			eval.room_internal?
				true:

			// allowed by options
			non_conform.has(event::conforms::MISMATCH_HASHES)?
				true:

			// allowed by room auth
			event.event_id?
				bool(m::redacted(event.event_id)):

			// otherwise deny
			false
		};

		auto report
		{
			eval.report
		};

		// When allowed, this hook won't throw, but the eval.report will
		// still indicate MISMATCH_HASHES.
		if(allow_redaction)
			report.del(event::conforms::MISMATCH_HASHES);

		if(!report.clean())
			throw error
			{
				fault::INVALID, "Non-conforming event: %s",
				string(report)
			};
	}
};

namespace ircd::m
{
	constexpr size_t event_conforms_num{num_of<event::conforms::code>()};
	extern const std::array<string_view, event_conforms_num> event_conforms_reflects;
}

decltype(ircd::m::event_conforms_reflects)
ircd::m::event_conforms_reflects
{
	"INVALID_OR_MISSING_EVENT_ID",
	"INVALID_OR_MISSING_ROOM_ID",
	"INVALID_OR_MISSING_SENDER_ID",
	"MISSING_TYPE",
	"INVALID_TYPE",
	"MISSING_ORIGIN",
	"INVALID_ORIGIN",
	"INVALID_STATE_KEY",
	"INVALID_OR_MISSING_REDACTS_ID",
	"MISSING_CONTENT_MEMBERSHIP",
	"INVALID_CONTENT_MEMBERSHIP",
	"MISSING_MEMBER_STATE_KEY",
	"INVALID_MEMBER_STATE_KEY",
	"MISSING_PREV_EVENTS",
	"MISSING_AUTH_EVENTS",
	"DEPTH_NEGATIVE",
	"DEPTH_ZERO",
	"MISSING_SIGNATURES",
	"MISSING_ORIGIN_SIGNATURE",
	"MISMATCH_ORIGIN_SENDER",
	"MISMATCH_CREATE_SENDER",
	"MISMATCH_ALIASES_STATE_KEY",
	"SELF_REDACTS",
	"SELF_PREV_EVENT",
	"SELF_AUTH_EVENT",
	"DUP_PREV_EVENT",
	"DUP_AUTH_EVENT",
	"MISMATCH_EVENT_ID",
	"MISSING_HASHES",
	"MISMATCH_HASHES",
};

std::ostream &
ircd::m::operator<<(std::ostream &s, const event::conforms &conforms)
{
	thread_local char buf[1024];
	s << conforms.string(buf);
	return s;
}

ircd::string_view
ircd::m::reflect(const event::conforms::code &code)
try
{
	return event_conforms_reflects.at(code);
}
catch(const std::out_of_range &e)
{
	return "??????"_sv;
}

ircd::m::event::conforms::code
ircd::m::event::conforms::reflect(const string_view &name)
{
	const auto it
	{
		std::find(begin(event_conforms_reflects), end(event_conforms_reflects), name)
	};

	if(it == end(event_conforms_reflects))
		throw std::out_of_range
		{
			"There is no event::conforms code by that name."
		};

	return code(std::distance(begin(event_conforms_reflects), it));
}

ircd::m::event::conforms::conforms(const event &e,
                                   const uint64_t &skip)
:conforms{e}
{
	report &= ~skip;
}

ircd::m::event::conforms::conforms(const event &e)
try
:report{0}
{
	if(!e.event_id)
		set(INVALID_OR_MISSING_EVENT_ID);

	if(defined(json::get<"event_id"_>(e)))
		if(!valid(m::id::EVENT, json::get<"event_id"_>(e)))
			set(INVALID_OR_MISSING_EVENT_ID);

	if(!has(INVALID_OR_MISSING_EVENT_ID))
		if(!m::check_id(e))
			set(MISMATCH_EVENT_ID);

	if(empty(json::get<"hashes"_>(e)))
		set(MISSING_HASHES);

	if(!has(MISMATCH_HASHES) && !has(MISSING_HASHES))
		if(!m::verify_hash(e))
			set(MISMATCH_HASHES);

	if(!valid(m::id::ROOM, json::get<"room_id"_>(e)))
		set(INVALID_OR_MISSING_ROOM_ID);

	if(!valid(m::id::USER, json::get<"sender"_>(e)))
		set(INVALID_OR_MISSING_SENDER_ID);

	if(empty(json::get<"type"_>(e)))
		set(MISSING_TYPE);

	if(json::get<"type"_>(e).size() > event::TYPE_MAX_SIZE)
		set(INVALID_TYPE);

	if(empty(json::get<"origin"_>(e)))
		set(MISSING_ORIGIN);

	if(json::get<"origin"_>(e).size() > event::ORIGIN_MAX_SIZE)
		set(INVALID_ORIGIN);

	if(!rfc3986::valid_remote(std::nothrow, json::get<"origin"_>(e)))
		set(INVALID_ORIGIN);

	if(json::get<"state_key"_>(e).size() > event::STATE_KEY_MAX_SIZE)
		set(INVALID_STATE_KEY);

	if(empty(json::get<"signatures"_>(e)))
		set(MISSING_SIGNATURES);

	if(empty(json::object{json::get<"signatures"_>(e).get(json::get<"origin"_>(e))}))
		set(MISSING_ORIGIN_SIGNATURE);

	if(!has(INVALID_OR_MISSING_SENDER_ID))
		if(json::get<"origin"_>(e) != m::id::user{json::get<"sender"_>(e)}.host())
			set(MISMATCH_ORIGIN_SENDER);

	if(json::get<"type"_>(e) == "m.room.create")
		if(m::room::id(json::get<"room_id"_>(e)).host() != m::user::id(json::get<"sender"_>(e)).host())
			set(MISMATCH_CREATE_SENDER);

	if(json::get<"type"_>(e) == "m.room.aliases")
		if(m::user::id(json::get<"sender"_>(e)).host() != json::get<"state_key"_>(e))
			set(MISMATCH_ALIASES_STATE_KEY);

	if(json::get<"type"_>(e) == "m.room.redaction")
		if(!valid(m::id::EVENT, json::get<"redacts"_>(e)))
			set(INVALID_OR_MISSING_REDACTS_ID);

	if(json::get<"redacts"_>(e))
		if(json::get<"redacts"_>(e) == e.event_id)
			set(SELF_REDACTS);

	if(json::get<"type"_>(e) == "m.room.member")
		if(empty(unquote(json::get<"content"_>(e).get("membership"))))
			set(MISSING_CONTENT_MEMBERSHIP);

	if(json::get<"type"_>(e) == "m.room.member")
		if(!all_of<std::islower>(unquote(json::get<"content"_>(e).get("membership"))))
			set(INVALID_CONTENT_MEMBERSHIP);

	if(json::get<"type"_>(e) == "m.room.member")
		if(empty(json::get<"state_key"_>(e)))
			set(MISSING_MEMBER_STATE_KEY);

	if(json::get<"type"_>(e) == "m.room.member")
		if(!valid(m::id::USER, json::get<"state_key"_>(e)))
			set(INVALID_MEMBER_STATE_KEY);

	if(json::get<"type"_>(e) != "m.room.create")
		if(empty(json::get<"prev_events"_>(e)))
			set(MISSING_PREV_EVENTS);

	if(json::get<"type"_>(e) != "m.room.create")
		if(empty(json::get<"auth_events"_>(e)))
			set(MISSING_AUTH_EVENTS);

	if(json::get<"depth"_>(e) != json::undefined_number && json::get<"depth"_>(e) < 0)
		set(DEPTH_NEGATIVE);

	if(json::get<"type"_>(e) != "m.room.create")
		if(json::get<"depth"_>(e) == 0)
			set(DEPTH_ZERO);

	const event::prev prev{e};
	const event::auth auth{e};
	if(json::get<"event_id"_>(e))
	{
		for(size_t i(0); i < auth.auth_events_count(); ++i)
			if(auth.auth_event(i) == json::get<"event_id"_>(e))
				set(SELF_AUTH_EVENT);

		for(size_t i(0); i < prev.prev_events_count(); ++i)
			if(prev.prev_event(i) == json::get<"event_id"_>(e))
				set(SELF_PREV_EVENT);
	}

	for(size_t i(0); i < auth.auth_events_count(); ++i)
	{
		const auto &[event_id, ref_hash]
		{
			auth.auth_events(i)
		};

		for(size_t j(0); j < auth.auth_events_count(); ++j)
			if(i != j)
				if(event_id == auth.auth_event(j))
					set(DUP_AUTH_EVENT);
	}

	for(size_t i(0); i < prev.prev_events_count(); ++i)
	{
		const auto &[event_id, ref_hash]
		{
			prev.prev_events(i)
		};

		for(size_t j(0); j < prev.prev_events_count(); ++j)
			if(i != j)
				if(event_id == prev.prev_event(j))
					set(DUP_PREV_EVENT);
	}
}
catch(const std::exception &_e)
{
	log::error
	{
		log, "Unable to complete conformity check :%s",
		_e.what(),
	};

	throw;
}

void
ircd::m::event::conforms::operator|=(const code &code)
&
{
	set(code);
}

void
ircd::m::event::conforms::del(const code &code)
{
	report &= ~(1UL << code);
}

void
ircd::m::event::conforms::set(const code &code)
{
	report |= (1UL << code);
}

ircd::string_view
ircd::m::event::conforms::string(const mutable_buffer &out)
const
{
	mutable_buffer buf{out};
	for(uint64_t i(0); i < num_of<code>(); ++i)
	{
		if(!has(code(i)))
			continue;

		if(begin(buf) != begin(out))
			consume(buf, copy(buf, ' '));

		consume(buf, copy(buf, m::reflect(code(i))));
	}

	return { data(out), begin(buf) };
}

bool
ircd::m::event::conforms::has(const code &code)
const
{
	return report & (1UL << code);
}

bool
ircd::m::event::conforms::has(const uint &code)
const
{
	return (report & (1UL << code)) == code;
}

bool
ircd::m::event::conforms::operator!()
const
{
	return clean();
}

ircd::m::event::conforms::operator bool()
const
{
	return !clean();
}

bool
ircd::m::event::conforms::clean()
const
{
	return report == 0;
}
