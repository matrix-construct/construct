// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
    static void auth_room_redaction(const event &, event::auth::hookdata &);
    extern hookfn<event::auth::hookdata &> auth_room_redaction_hookfn;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix m.room.redaction"
};

decltype(ircd::m::auth_room_redaction_hookfn)
ircd::m::auth_room_redaction_hookfn
{
	auth_room_redaction,
	{
		{ "_site",    "event.auth"        },
		{ "type",     "m.room.redaction"  },
	}
};

void
ircd::m::auth_room_redaction(const m::event &event,
                                event::auth::hookdata &data)
{
	using FAIL = event::auth::FAIL;

	// 11. If type is m.room.redaction:
	assert(json::get<"type"_>(event) == "m.room.redaction");

	const m::event::prev prev{event};
	const m::room::power power
	{
		data.auth_power? *data.auth_power : m::event{}, *data.auth_create
	};

	// a. If the sender's power level is greater than or equal to the
	// redact level, allow.
	if(power(at<"sender"_>(event), "redact"))
	{
		data.allow = true;
		return;
	}

	// b. If the domain of the event_id of the event being redacted is the
	// same as the domain of the event_id of the m.room.redaction, allow.
	//
	//if(event::id(json::get<"redacts"_>(event)).host() == user::id(at<"sender"_>(event)).host())
	//	return {};
	//
	// In past room versions, redactions were only permitted to enter the
	// DAG if the sender's domain matched the domain in the event ID
	// being redacted, or the sender had appropriate permissions per the
	// power levels. Due to servers now not being able to determine where
	// an event came from during event authorization, redaction events
	// are always accepted (provided the event is allowed by events and
	// events_default in the power levels). However, servers should not
	// apply or send redactions to clients until both the redaction event
	// and original event have been seen, and are valid. Servers should
	// only apply redactions to events where the sender's domains match,
	// or the sender of the redaction has the appropriate permissions per
	// the power levels.
	const auto redact_target_idx
	{
		m::index(at<"redacts"_>(event), std::nothrow)
	};

	if(!redact_target_idx)
		throw FAIL
		{
			"m.room.redaction redacts target is unknown."
		};

	const auto target_in_room
	{
		[&event](const string_view &room_id)
		{
			return room_id == at<"room_id"_>(event);
		}
	};

	if(!m::query(std::nothrow, redact_target_idx, "room_id", target_in_room))
		throw FAIL
		{
			"m.room.redaction redacts target is not in room."
		};

	const auto sender_domain_match
	{
		[&event](const string_view &tgt)
		{
			return tgt? user::id(tgt).host() == user::id(at<"sender"_>(event)).host(): false;
		}
	};

	if(m::query(std::nothrow, redact_target_idx, "redacts", sender_domain_match))
	{
		data.allow = true;
		return;
	}

	// c. Otherwise, reject.
	throw FAIL
	{
		"m.room.redaction fails authorization."
	};
}
