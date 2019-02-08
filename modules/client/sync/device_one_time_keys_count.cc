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
	"Client Sync :Device One Time Keys Count"
};

namespace ircd::m::sync
{
	static void device_one_time_keys_count_polylog(data &);
	static void device_one_time_keys_count_linear(data &);

	extern item device_one_time_keys_count;
}

decltype(ircd::m::sync::device_one_time_keys_count)
ircd::m::sync::device_one_time_keys_count
{
	"device_one_time_keys_count",
	device_one_time_keys_count_polylog,
	device_one_time_keys_count_linear
};

void
ircd::m::sync::device_one_time_keys_count_linear(data &data)
{

}

void
ircd::m::sync::device_one_time_keys_count_polylog(data &data)
{
	json::stack::object object
	{
		data.out
	};

}
