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
    "Client Sync :Room Receipts"
};

namespace ircd::m::sync
{
	static bool room_ephemeral_m_receipt_m_read_polylog(data &);
	extern item room_ephemeral_m_receipt_m_read;
}

decltype(ircd::m::sync::room_ephemeral_m_receipt_m_read)
ircd::m::sync::room_ephemeral_m_receipt_m_read
{
	"rooms...ephemeral",
	room_ephemeral_m_receipt_m_read_polylog
};

bool
ircd::m::sync::room_ephemeral_m_receipt_m_read_polylog(data &data)
{
	const m::room &room{*data.room};
	const m::room::members members{room};
	const m::room::members::closure closure{[&]
	(const m::user::id &user_id)
	{
		static const m::event::fetch::opts fopts
		{
			m::event::keys::include
			{
				"event_id",
				"content",
				"sender",
			},
		};

		const m::user user{user_id};
		m::user::room user_room{user};
		user_room.fopts = &fopts;
		if(head_idx(std::nothrow, user_room) <= data.since)
			return;

		user_room.get(std::nothrow, "ircd.read", room.room_id, [&]
		(const m::event &event)
		{
			const auto &event_idx(index(event, std::nothrow));
			if(event_idx < data.since || event_idx >= data.current)
				return;

			data.commit();
			json::stack::object object{*data.array};

			// type
			json::stack::member
			{
				object, "type", "m.receipt"
			};

			// content
			const json::object data
			{
				at<"content"_>(event)
			};

			thread_local char buf[1024];
			const json::members reformat
			{
				{ unquote(data.at("event_id")),
				{
					{ "m.read",
					{
						{ at<"sender"_>(event),
						{
							{ "ts", data.at("ts") }
						}}
					}}
				}}
			};

			json::stack::member
			{
				object, "content", json::stringify(mutable_buffer{buf}, reformat)
			};
		});
	}};

	return true;
}
