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
	std::map<std::string, long>
	count_one_time_keys(const m::user::id &, const m::device::id &);
}

namespace ircd::m::sync
{
	static bool _device_one_time_keys_count(data &);
	static bool device_one_time_keys_count_polylog(data &);
	static bool device_one_time_keys_count_linear(data &);

	extern item device_one_time_keys_count;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client Sync :Device One Time Keys Count"
};

decltype(ircd::m::sync::device_one_time_keys_count)
ircd::m::sync::device_one_time_keys_count
{
	"device_one_time_keys_count",
	device_one_time_keys_count_polylog,
	device_one_time_keys_count_linear
};

bool
ircd::m::sync::device_one_time_keys_count_linear(data &data)
{
	if(!data.device_id)
		return false;

	if(!data.event || !data.event->event_id)
		return false;

	if(!startswith(json::get<"type"_>(*data.event), "ircd.device.one_time_key"))
		return false;

	if(json::get<"state_key"_>(*data.event) != data.device_id)
		return false;

	if(json::get<"room_id"_>(*data.event) != data.user_room)
		return false;

	const json::object &one_time_keys
	{
		json::get<"content"_>(*data.event)
	};

	json::stack::object object
	{
		*data.out, "device_one_time_keys_count"
	};

	return _device_one_time_keys_count(data);
}

bool
ircd::m::sync::device_one_time_keys_count_polylog(data &data)
{
	if(!data.device_id)
		return false;

	return _device_one_time_keys_count(data);
}

bool
ircd::m::sync::_device_one_time_keys_count(data &data)
{
	const auto counts
	{
		m::user::devices::count_one_time_keys(data.user, data.device_id)
	};

	for(const auto &[algorithm, count] : counts)
		json::stack::member
		{
			*data.out, algorithm, json::value{count}
		};

	return true;
}
