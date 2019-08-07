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
	static bool device_one_time_keys_count_polylog(data &);
	static bool device_one_time_keys_count_linear(data &);

	extern item device_one_time_keys_count;
}

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

	return device_one_time_keys_count_polylog(data);
}

bool
ircd::m::sync::device_one_time_keys_count_polylog(data &data)
{
	if(!data.device_id)
		return false;

	const auto event_idx
	{
		data.user_room.get(std::nothrow, "ircd.device_one_time_keys", data.device_id)
	};

	if(!apropos(data, event_idx))
		return false;

	std::map<string_view, long, std::less<>> counts;
	m::device::get(data.user, data.device_id, "one_time_keys", [&counts]
	(const string_view &one_time_keys)
	{
		if(json::type(one_time_keys) != json::OBJECT)
			return;

		for(const auto &[ident_, object] : json::object(one_time_keys))
		{
			const auto &[algorithm, ident]
			{
				split(ident_, ':')
			};

			auto it(counts.lower_bound(algorithm));
			if(it == end(counts) || it->first != algorithm)
				it = counts.emplace_hint(it, algorithm, 0L);

			auto &count(it->second);
			++count;
		}
	});

	json::stack::object one_time_keys_count
	{
		*data.out, "device_one_time_keys_count"
	};

	for(const auto &[algorithm, count] : counts)
		json::stack::member
		{
			one_time_keys_count, algorithm, json::value{count}
		};

	return true;
}
