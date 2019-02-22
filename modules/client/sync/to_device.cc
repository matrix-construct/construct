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

	const m::room &user_room
	{
		data.user_room
	};

	m::room::messages it
	{
		user_room
	};

	for(; it; ++it)
	{
		const auto &event_idx(it.event_idx());
		if(!apropos(data, event_idx))
			break;

		bool relevant(false);
		m::get(std::nothrow, event_idx, "type", [&relevant]
		(const string_view &type)
		{
			relevant = type == "ircd.to_device";
		});

		if(!relevant)
			continue;

		m::get(std::nothrow, event_idx, "content", [&data, &array]
		(const json::object &content)
		{
			data.commit();
			json::stack::object event
			{
				array
			};

			json::stack::member
			{
				event, "sender", unquote(content.at("sender"))
			};

			json::stack::member
			{
				event, "type", unquote(content.at("type"))
			};

			json::stack::object content_
			{
				event, "content"
			};

			json::stack::member
			{
				content_, "device_id", unquote(content.at("device_id"))
			};

			for(const auto &[property, value] : json::object(content.at("content")))
				json::stack::member
				{
					content_, property, value
				};
		});
	}
}
