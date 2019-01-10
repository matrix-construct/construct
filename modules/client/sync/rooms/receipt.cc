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
	static void _reformat_receipt(json::stack::object &, const m::event &);
	static void _handle_receipt(data &, const m::event &);
	static void _handle_user(data &, const m::user &);
	static void room_ephemeral_m_receipt_m_read_polylog(data &);
	extern item room_ephemeral_m_receipt_m_read;
}

decltype(ircd::m::sync::room_ephemeral_m_receipt_m_read)
ircd::m::sync::room_ephemeral_m_receipt_m_read
{
	"rooms.ephemeral.m_receipt",
	room_ephemeral_m_receipt_m_read_polylog
};

void
ircd::m::sync::room_ephemeral_m_receipt_m_read_polylog(data &data)
{
	const m::room &room{*data.room};
	const m::room::members members
	{
		room
	};

	members.for_each(data.membership, m::room::members::closure{[&data]
	(const m::user::id &user_id)
	{
		_handle_user(data, user_id);
	}});
}

void
ircd::m::sync::_handle_user(data &data,
                            const m::user &user)
{
	static const m::event::fetch::opts fopts
	{
		m::event::keys::include
		{
			"event_id", "content", "sender",
		},
	};

	m::user::room user_room{user};
	user_room.fopts = &fopts;
	if(head_idx(std::nothrow, user_room) < data.range.first)
		return;

	const m::room::id &room_id{*data.room};
	user_room.get(std::nothrow, "ircd.read", room_id, [&data]
	(const m::event &event)
	{
		if(apropos(data, event))
			_handle_receipt(data, event);
	});
}

void
ircd::m::sync::_handle_receipt(data &data,
                               const m::event &event)
{
	const json::object content
	{
		at<"content"_>(event)
	};

	data.commit();
	json::stack::object object
	{
		data.out
	};

	// type
	json::stack::member
	{
		object, "type", "m.receipt"
	};

	// content
	json::stack::object content_
	{
		object, "content"
	};

	json::stack::object event_id_
	{
		content_, unquote(content.at("event_id"))
	};

	json::stack::object m_read_
	{
		event_id_, "m.read"
	};

	json::stack::object sender_
	{
		m_read_, at<"sender"_>(event)
	};

	json::stack::member
	{
		sender_, "ts", json::value(content.at("ts"))
	};
}
