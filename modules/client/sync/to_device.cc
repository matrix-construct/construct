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
	static void _to_device_append(data &, const json::object &, json::stack::array &);
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
	if(!data.event_idx)
		return false;

	assert(data.event);
	const m::event &event{*data.event};
	if(json::get<"room_id"_>(event) != data.user_room.room_id)
		return false;

	if(json::get<"type"_>(event) != "ircd.to_device")
		return false;

	const json::object &content
	{
		json::get<"content"_>(event)
	};

	const json::string &device_id
	{
		content.at("device_id")
	};

	if(device_id != "*" && device_id != data.device_id)
		return false;

	json::stack::object to_device
	{
		*data.out, "to_device"
	};

	json::stack::array array
	{
		to_device, "events"
	};

	_to_device_append(data, content, array);
	return true;
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

			if(device_id != "*" && device_id != data.device_id)
				return;

			_to_device_append(data, content, array);
			ret = true;
		});

		return true;
	});

	return ret;
}

void
ircd::m::sync::_to_device_append(data &data,
                                 const json::object &content,
                                 json::stack::array &array)
{
	json::stack::object event
	{
		array
	};

	json::stack::member
	{
		event, "sender", json::string
		{
			content.at("sender")
		}
	};

	json::stack::member
	{
		event, "type", json::string
		{
			content.at("type")
		}
	};

	json::stack::member
	{
		event, "content", json::object
		{
			content.at("content")
		}
	};
}
