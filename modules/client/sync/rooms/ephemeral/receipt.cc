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
    "Client Sync :Room Ephemeral :Receipts"
};

namespace ircd::m::sync
{
	static void _reformat_receipt(json::stack::object &, const m::event &);
	static void _handle_receipt(data &, const m::event &);
	static void _handle_user(data &, const m::user &, ctx::mutex &);
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

	static const size_t fibers(64); //TODO: conf
	using queue = std::array<string_view, fibers>;
	using buffer = std::array<char[m::id::MAX_SIZE+1], fibers>;

	queue q;
	const auto buf
	{
		std::make_unique<buffer>()
	};

	ctx::mutex mutex;
	ctx::parallel<string_view> parallel
	{
		m::sync::pool, q, [&data, &mutex]
		(const m::user::id user_id)
		{
			const m::user user{user_id};
			_handle_user(data, user, mutex);
		}
	};

	members.for_each("join", m::room::members::closure{[&parallel, &q, &buf]
	(const m::user::id &user_id)
	{
		const auto pos(parallel.nextpos());
		q[pos] = strlcpy(buf->at(pos), user_id);
		parallel();
	}});
}

void
ircd::m::sync::_handle_user(data &data,
                            const m::user &user,
                            ctx::mutex &mutex)
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
	user_room.get(std::nothrow, "ircd.read", room_id, [&data, &mutex]
	(const m::event &event)
	{
		if(apropos(data, event))
		{
			data.commit();
			const std::lock_guard<decltype(mutex)> lock(mutex);
			_handle_receipt(data, event);
		}
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
