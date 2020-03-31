// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::sync
{
	static bool to_device_polylog(data &);
	static bool to_device_linear(data &);

	extern item to_device;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client Sync :To Device"
};

decltype(ircd::m::sync::to_device)
ircd::m::sync::to_device
{
	"to_device",
	to_device_polylog,
	to_device_linear
};

bool
ircd::m::sync::to_device_linear(data &data)
{
	return false;
}

bool
ircd::m::sync::to_device_polylog(data &data)
{
	json::stack::array array
	{
		*data.out, "events"
	};

	m::room::type events
	{
		data.user_room, "ircd.to_device",
		{
			-1UL, data.range.first
		}
	};

	bool ret{false};
	events.for_each([&data, &array, &ret]
	(const string_view &type, const uint64_t &depth, const event::idx &event_idx)
	{
		m::get(std::nothrow, event_idx, "content", [&data, &array, &ret]
		(const json::object &content)
		{
			const json::string &device_id
			{
				content.at("device_id")
			};

			if(device_id != data.device_id)
				return;

			const json::string &sender
			{
				content.at("sender")
			};

			const json::string &type
			{
				content.at("type")
			};

			json::stack::object event
			{
				array
			};

			json::stack::member
			{
				event, "sender", sender
			};

			json::stack::member
			{
				event, "type", type
			};

			json::stack::object content_
			{
				event, "content"
			};

			json::stack::member
			{
				content_, "device_id", device_id
			};

			for(const auto &[property, value] : json::object(content.at("content")))
				json::stack::member
				{
					content_, property, value
				};

			ret = true;
		});

		return true;
	});

	return ret;
}
