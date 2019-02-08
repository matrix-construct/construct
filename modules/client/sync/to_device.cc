// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::mapi::header
IRCD_MODULE
{
	"Client Sync :To Device"
};

namespace ircd::m::sync
{
	static void to_device_polylog(data &);
	static void to_device_linear(data &);

	extern item to_device;
}

decltype(ircd::m::sync::to_device)
ircd::m::sync::to_device
{
	"to_device",
	to_device_polylog,
	to_device_linear
};

void
ircd::m::sync::to_device_linear(data &data)
{

}

void
ircd::m::sync::to_device_polylog(data &data)
{
	json::stack::object object
	{
		data.out
	};

	json::stack::array array
	{
		data.out, "events"
	};

	const m::room::state &state
	{
		data.user_state
	};
}
