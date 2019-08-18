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
	static void auth_room_third_party_invite(const event &, room::auth::hookdata &);
	extern hookfn<room::auth::hookdata &> auth_room_third_party_invite_hookfn;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix m.room.third_party_invite"
};

//
// auth handler
//

decltype(ircd::m::auth_room_third_party_invite_hookfn)
ircd::m::auth_room_third_party_invite_hookfn
{
	auth_room_third_party_invite,
	{
		{ "_site",    "room.auth"                  },
		{ "type",     "m.room.third_party_invite"  },
	}
};

void
ircd::m::auth_room_third_party_invite(const event &event,
                                      room::auth::hookdata &data)
{
	using FAIL = m::room::auth::FAIL;

	// 7. If type is m.room.third_party_invite:
	assert(json::get<"type"_>(event) == "m.room.third_party_invite");

	const m::room::power power
	{
		data.auth_power? *data.auth_power : m::event{}, *data.auth_create
	};

	// a. Allow if and only if sender's current power level is greater
	// than or equal to the invite level.
	if(power(at<"sender"_>(event), "invite"))
	{
		data.allow = true;
		return;
	}

	throw FAIL
	{
		"sender has power level less than required for invite."
	};
}
