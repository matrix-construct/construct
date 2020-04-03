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
	extern const m::event::fetch::opts receipt_fopts;
	extern conf::item<int64_t> receipt_scan_depth;

	static bool _handle_message_receipt(data &, const m::event &);
	static bool _handle_message(data &, const m::event::idx &);
	static size_t _prefetch_message(data &, const m::event::idx &);
	static bool room_ephemeral_m_receipt_m_read_polylog(data &);
	static bool room_ephemeral_m_receipt_m_read_linear(data &);
	extern item room_ephemeral_m_receipt_m_read;
}

ircd::mapi::header
IRCD_MODULE
{
    "Client Sync :Room Ephemeral :Receipts"
};

decltype(ircd::m::sync::room_ephemeral_m_receipt_m_read)
ircd::m::sync::room_ephemeral_m_receipt_m_read
{
	"rooms.ephemeral.m_receipt",
	room_ephemeral_m_receipt_m_read_polylog,
	room_ephemeral_m_receipt_m_read_linear,
	{
		{ "phased", true }
	}
};

decltype(ircd::m::sync::receipt_scan_depth)
ircd::m::sync::receipt_scan_depth
{
	{ "name",     "ircd.client.sync.rooms.ephemeral.receipt_scan_depth" },
	{ "default",  10L                                                   },
};

decltype(ircd::m::sync::receipt_fopts)
ircd::m::sync::receipt_fopts
{
	m::event::keys::include { "content", "sender" }
};

bool
ircd::m::sync::room_ephemeral_m_receipt_m_read_linear(data &data)
{
	assert(data.event);
	if(json::get<"type"_>(*data.event) != "ircd.read")
		return false;

	if(json::get<"sender"_>(*data.event) == data.user.user_id)
		return false;

	const m::room room
	{
		json::get<"state_key"_>(*data.event)
	};

	if(!membership(room, data.user, "join"))
		return false;

	json::stack::object rooms
	{
		*data.out, "rooms"
	};

	json::stack::object membership_
	{
		*data.out, "join"
	};

	json::stack::object room_
	{
		*data.out, room.room_id
	};

	json::stack::object ephemeral
	{
		*data.out, "ephemeral"
	};

	json::stack::array events
	{
		*data.out, "events"
	};

	_handle_message_receipt(data, *data.event);
	return true;
}

bool
ircd::m::sync::room_ephemeral_m_receipt_m_read_polylog(data &data)
{
	// With this sync::item being phased=true, this gets called for initial (zero)
	// and all negative phases. We don't want to incur this load during initial.
	if(data.phased && int64_t(data.range.first) == 0L)
		return false;

	m::room::events it
	{
		*data.room
	};

	// Prefetch loop for event::refs; initial recent messages walk.
	ssize_t i(0);
	event::idx idx(0);
	for(; it && i < receipt_scan_depth; --it)
	{
		if(!apropos(data, it.event_idx()) && i > 0)
			break;

		const event::refs refs{it.event_idx()};
		refs.prefetch(dbs::ref::M_RECEIPT__M_READ);
		idx = it.event_idx();
		++i;
	}

	if(i > 0)
		it.seek();

	// Prefetch loop for the receipt events
	for(ssize_t j(0); it && j < i; --it)
	{
		if(!apropos(data, it.event_idx()))
			continue;

		_prefetch_message(data, it.event_idx());
		++j;
	}

	if(i > 0)
		it.seek_idx(idx);

	// Fetch loop; stream to client.
	bool ret{false};
	for(ssize_t j(0); it && j < i; ++it)
	{
		if(!apropos(data, it.event_idx()))
			continue;

		ret |= _handle_message(data, it.event_idx());
		++j;
	}

	return ret;
}

size_t
ircd::m::sync::_prefetch_message(data &data,
                                 const event::idx &idx)
{
	size_t ret(0);
	const event::refs refs{idx};
	refs.for_each(dbs::ref::M_RECEIPT__M_READ, [&ret]
	(const event::idx &idx, const auto &type)
	{
		assert(type == dbs::ref::M_RECEIPT__M_READ);
		ret += m::prefetch(idx, receipt_fopts);
		return true;
	});

	return ret;
}

bool
ircd::m::sync::_handle_message(data &data,
                               const event::idx &idx)
{
	bool ret{false};
	const event::refs refs{idx};
	refs.for_each(dbs::ref::M_RECEIPT__M_READ, [&data, &ret]
	(const event::idx &idx, const auto &type)
	{
		assert(type == dbs::ref::M_RECEIPT__M_READ);
		const m::event::fetch event
		{
			std::nothrow, idx, receipt_fopts
		};

		if(event.valid)
			ret |= _handle_message_receipt(data, event);

		return true;
	});

	return ret;
}

bool
ircd::m::sync::_handle_message_receipt(data &data,
                                       const m::event &event)
{
	const json::object content
	{
		at<"content"_>(event)
	};

	json::stack::object object
	{
		*data.out
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
		content_, json::string
		{
			content.at("event_id")
		}
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
		sender_, "ts", json::value
		{
			content.at("ts")
		}
	};

	return true;
}
