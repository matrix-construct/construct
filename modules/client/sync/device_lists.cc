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
	"Client Sync :Device Lists"
};

namespace ircd::m::sync
{
	static bool device_lists_polylog(data &);
	static bool device_lists_linear(data &);

	extern item device_lists;
}

decltype(ircd::m::sync::device_lists)
ircd::m::sync::device_lists
{
	"device_lists",
	device_lists_polylog,
	device_lists_linear
};

bool
ircd::m::sync::device_lists_linear(data &data)
{
	return false;
}

bool
ircd::m::sync::device_lists_polylog(data &data)
{
	json::stack::array
	{
		data.out, "changed"
	};

	json::stack::array
	{
		data.out, "left"
	};

	return false;
}
