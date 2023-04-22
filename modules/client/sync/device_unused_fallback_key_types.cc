// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::sync
{
	static bool device_unused_fallback_key_types_polylog(data &);
	static bool device_unused_fallback_key_types_linear(data &);

	extern item device_unused_fallback_key_types;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client Sync :Device Unused Fallback Key Types"
};

decltype(ircd::m::sync::device_unused_fallback_key_types)
ircd::m::sync::device_unused_fallback_key_types
{
	"device_unused_fallback_key_types",
	device_unused_fallback_key_types_polylog,
	device_unused_fallback_key_types_linear
};

bool
ircd::m::sync::device_unused_fallback_key_types_linear(data &data)
{
	if(!data.device_id)
		return false;

	if(!data.event || !data.event->event_id)
		return false;

	if(!startswith(json::get<"type"_>(*data.event), "ircd.device"))
		return false;

	if(json::get<"room_id"_>(*data.event) != data.user_room)
		return false;

	json::stack::array array
	{
		*data.out, "device_unused_fallback_key_types"
	};

	array.append("signed_curve25519");
	return true;
}

bool
ircd::m::sync::device_unused_fallback_key_types_polylog(data &data)
{
	if(!data.device_id)
		return false;

	return false;
}
