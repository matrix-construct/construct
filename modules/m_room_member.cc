// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static void auth_room_member_ban(const event &, room::auth::hookdata &);
	extern m::hookfn<room::auth::hookdata &> auth_room_member_ban_hookfn;

	static void auth_room_member_leave(const event &, room::auth::hookdata &);
	extern m::hookfn<room::auth::hookdata &> auth_room_member_leave_hookfn;

	static void auth_room_member_invite(const event &, room::auth::hookdata &);
	extern m::hookfn<room::auth::hookdata &> auth_room_member_invite_hookfn;

	static void auth_room_member_join(const event &, room::auth::hookdata &);
	extern m::hookfn<room::auth::hookdata &> auth_room_member_join_hookfn;

	static void auth_room_member(const event &, room::auth::hookdata &);
	extern m::hookfn<room::auth::hookdata &> auth_room_member_hookfn;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix m.room.member"
};

decltype(ircd::m::auth_room_member_hookfn)
ircd::m::auth_room_member_hookfn
{
	auth_room_member,
	{
		{ "_site",  "room.auth"     },
		{ "type",   "m.room.member" },
	}
};

void
ircd::m::auth_room_member(const m::event &event,
                          room::auth::hookdata &data)
{
	using FAIL = m::room::auth::FAIL;

	// 5. If type is m.room.member:
	assert(json::get<"type"_>(event) == "m.room.member");

	// a. If no state_key key ...
	if(empty(json::get<"state_key"_>(event)))
		throw FAIL
		{
			"m.room.member event is missing a state_key."
		};

	// a. ... or membership key in content, reject.
	if(empty(unquote(json::get<"content"_>(event).get("membership"))))
		throw FAIL
		{
			"m.room.member event is missing a content.membership."
		};

	if(!valid(m::id::USER, json::get<"state_key"_>(event)))
		throw FAIL
		{
			"m.room.member event state_key is not a valid user mxid."
		};

	const auto &membership
	{
		m::membership(event)
	};

	// b. If membership is join
	if(membership == "join")
		return; // see hook handler

	// c. If membership is invite
	if(membership == "invite")
		return; // see hook handler

	// d. If membership is leave
	if(membership == "leave")
		return; // see hook handler

	// e. If membership is ban
	if(membership == "ban")
		return; // see hook handler

	// f. Otherwise, the membership is unknown. Reject.
	throw FAIL
	{
		"m.room.member membership=unknown."
	};
}

decltype(ircd::m::auth_room_member_join_hookfn)
ircd::m::auth_room_member_join_hookfn
{
	auth_room_member_join,
	{
		{ "_site",       "room.auth"     },
		{ "type",        "m.room.member" },
		{ "content",
		{
			{ "membership",  "join" },
		}},
	}
};

void
ircd::m::auth_room_member_join(const m::event &event,
                               room::auth::hookdata &data)
{
	using FAIL = m::room::auth::FAIL;

	// b. If membership is join
	assert(membership(event) == "join");

	// i. If the only previous event is an m.room.create and the
	// state_key is the creator, allow.
	const m::event::prev prev(event);
	const m::event::auth auth(event);
	if(prev.prev_events_count() == 1 && auth.auth_events_count() == 1)
		if(data.auth_create && data.auth_create->event_id == prev.prev_event(0))
		{
			data.allow = true;
			return;
		}

	// ii. If the sender does not match state_key, reject.
	if(json::get<"sender"_>(event) != json::get<"state_key"_>(event))
		throw FAIL
		{
			"m.room.member membership=join sender does not match state_key."
		};

	// iii. If the sender is banned, reject.
	if(data.auth_member_sender)
		if(membership(*data.auth_member_sender) == "ban")
			throw FAIL
			{
				"m.room.member membership=join references membership=ban auth_event."
			};

	const json::string &join_rule
	{
		data.auth_join_rules?
			json::get<"content"_>(*data.auth_join_rules).get("join_rule"):
			"invite"_sv
	};

	// iv. If the join_rule is invite then allow if membership state
	// is invite or join.
	if(join_rule == "invite")
	{
		if(!data.auth_member_target)
			throw FAIL
			{
				"m.room.member membership=join missing target member auth event."
			};

		const auto membership_state
		{
			membership(*data.auth_member_target)
		};

		if(membership_state == "invite")
		{
			data.allow = true;
			return;
		}

		if(membership_state == "join")
		{
			data.allow = true;
			return;
		}
	}

	// v. If the join_rule is public, allow.
	if(join_rule == "public")
	{
		data.allow = true;
		return;
	}

	// vi. Otherwise, reject.
	throw FAIL
	{
		"m.room.member membership=join fails authorization."
	};
}

decltype(ircd::m::auth_room_member_invite_hookfn)
ircd::m::auth_room_member_invite_hookfn
{
	auth_room_member_invite,
	{
		{ "_site",       "room.auth"     },
		{ "type",        "m.room.member" },
		{ "content",
		{
			{ "membership",  "invite" },
		}},
	}
};

void
ircd::m::auth_room_member_invite(const m::event &event,
                                 room::auth::hookdata &data)
{
	using FAIL = m::room::auth::FAIL;

	// c. If membership is invite
	assert(membership(event) == "invite");

	// i. If content has third_party_invite key
	if(json::get<"content"_>(event).has("third_party_invite"))
	{
		//TODO: XXX
		// 1. If target user is banned, reject.
		// 2. If content.third_party_invite does not have a signed key, reject.
		// 3. If signed does not have mxid and token keys, reject.
		// 4. If mxid does not match state_key, reject.
		// 5. If there is no m.room.third_party_invite event in the current room state with state_key matching token, reject.
		// 6. If sender does not match sender of the m.room.third_party_invite, reject.
		// 7. If any signature in signed matches any public key in the m.room.third_party_invite event, allow. The public keys are in content of m.room.third_party_invite as:
		// 7.1. A single public key in the public_key field.
		// 7.2. A list of public keys in the public_keys field.
		// 8. Otherwise, reject.
		throw FAIL
		{
			"third_party_invite fails authorization."
		};
	}

	if(!data.auth_member_sender)
		throw FAIL
		{
			"m.room.member membership=invite missing sender member auth event."
		};

	// ii. If the sender's current membership state is not join, reject.
	if(membership(*data.auth_member_sender) != "join")
		throw FAIL
		{
			"m.room.member membership=invite sender must have membership=join."
		};

	// iii. If target user's current membership state is join or ban, reject.
	if(data.auth_member_target)
		if(membership(*data.auth_member_target) == "join")
			throw FAIL
			{
				"m.room.member membership=invite target cannot have membership=join."
			};

	if(data.auth_member_target)
		if(membership(*data.auth_member_target) == "ban")
			throw FAIL
			{
				"m.room.member membership=invite target cannot have membership=ban."
			};

	// iv. If the sender's power level is greater than or equal to the invite level,
	// allow.
	const m::room::power power
	{
		data.auth_power? *data.auth_power : m::event{}, *data.auth_create
	};

	if(power(at<"sender"_>(event), "invite"))
	{
		data.allow = true;
		return;
	}

	// v. Otherwise, reject.
	throw FAIL
	{
		"m.room.member membership=invite fails authorization."
	};
}

decltype(ircd::m::auth_room_member_leave_hookfn)
ircd::m::auth_room_member_leave_hookfn
{
	auth_room_member_leave,
	{
		{ "_site",       "room.auth"     },
		{ "type",        "m.room.member" },
		{ "content",
		{
			{ "membership",  "leave" },
		}},
	}
};

void
ircd::m::auth_room_member_leave(const m::event &event,
                                room::auth::hookdata &data)
{
	using FAIL = m::room::auth::FAIL;

	// d. If membership is leave
	assert(membership(event) == "leave");

	// i. If the sender matches state_key, allow if and only if that
	// user's current membership state is invite or join.
	if(json::get<"sender"_>(event) == json::get<"state_key"_>(event))
	{
		if(data.auth_member_target && membership(*data.auth_member_target) == "join")
		{
			data.allow = true;
			return;
		}

		if(data.auth_member_target && membership(*data.auth_member_target) == "invite")
		{
			data.allow = true;
			return;
		}

		throw FAIL
		{
			"m.room.member membership=leave self-target must have membership=join|invite."
		};
	}

	if(!data.auth_member_sender)
		throw FAIL
		{
			"m.room.member membership=leave missing sender member auth event."
		};

	// ii. If the sender's current membership state is not join, reject.
	if(membership(*data.auth_member_sender) != "join")
		throw FAIL
		{
			"m.room.member membership=leave sender must have membership=join."
		};

	const m::room::power power
	{
		data.auth_power? *data.auth_power : m::event{}, *data.auth_create
	};

	if(!data.auth_member_target)
		throw FAIL
		{
			"m.room.member membership=leave missing target member auth event."
		};

	// iii. If the target user's current membership state is ban, and the sender's
	// power level is less than the ban level, reject.
	if(membership(*data.auth_member_target) == "ban")
		if(!power(at<"sender"_>(event), "ban"))
			throw FAIL
			{
				"m.room.member membership=ban->leave sender must have ban power to unban."
			};

	// iv. If the sender's power level is greater than or equal to the
	// kick level, and the target user's power level is less than the
	// sender's power level, allow.
	if(power(at<"sender"_>(event), "kick"))
		if(power.level_user(at<"state_key"_>(event)) < power.level_user(at<"sender"_>(event)))
		{
			data.allow = true;
			return;
		}

	// v. Otherwise, reject.
	throw FAIL
	{
		"m.room.member membership=leave fails authorization."
	};
}


decltype(ircd::m::auth_room_member_ban_hookfn)
ircd::m::auth_room_member_ban_hookfn
{
	auth_room_member_ban,
	{
		{ "_site",       "room.auth"     },
		{ "type",        "m.room.member" },
		{ "content",
		{
			{ "membership",  "ban" },
		}},
	}
};

void
ircd::m::auth_room_member_ban(const m::event &event,
                              room::auth::hookdata &data)
{
	using FAIL = m::room::auth::FAIL;

	// e. If membership is ban
	assert(membership(event) == "ban");

	if(!data.auth_member_sender)
		throw FAIL
		{
			"m.room.member membership=ban missing sender member auth event."
		};

	// i. If the sender's current membership state is not join, reject.
	if(membership(*data.auth_member_sender) != "join")
		throw FAIL
		{
			"m.room.member membership=ban sender must have membership=join."
		};

	const m::room::power power
	{
		data.auth_power? *data.auth_power : m::event{}, *data.auth_create
	};

	// ii. If the sender's power level is greater than or equal to the
	// ban level, and the target user's power level is less than the
	// sender's power level, allow.
	if(power(at<"sender"_>(event), "ban"))
		if(power.level_user(at<"state_key"_>(event)) < power.level_user(at<"sender"_>(event)))
		{
			data.allow = true;
			return;
		}

	// iii. Otherwise, reject.
	throw FAIL
	{
		"m.room.member membership=ban fails authorization."
	};
}
