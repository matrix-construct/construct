// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
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
				fault::INVALID, "Issuing event for origin :%s",
				at<"origin"_>(event)
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
			non_conform.has(event::conforms::MISMATCH_HASHES)
			|| opts.require_content == 0?
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
				fault::INVALID, "Non-conforming event :%s",
				string(report)
			};
	}
};
