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
	static bool _handle_message_receipt(data &, const m::event &);
	static bool _handle_message(data &, const m::event::idx &);
	static bool room_ephemeral_m_receipt_m_read_polylog(data &);
	static bool room_ephemeral_m_receipt_m_read_linear(data &);
	extern item room_ephemeral_m_receipt_m_read;
}

decltype(ircd::m::sync::room_ephemeral_m_receipt_m_read)
ircd::m::sync::room_ephemeral_m_receipt_m_read
{
	"rooms.ephemeral.m_receipt",
	room_ephemeral_m_receipt_m_read_polylog,
	room_ephemeral_m_receipt_m_read_linear
};

bool
ircd::m::sync::room_ephemeral_m_receipt_m_read_linear(data &data)
{
	assert(data.event);
	if(json::get<"type"_>(*data.event) != "m.receipt")
		return false;

	if(!my_host(json::get<"origin"_>(*data.event)))
		return false;

	_handle_message_receipt(data, *data.event);
	return true;
}

bool
ircd::m::sync::room_ephemeral_m_receipt_m_read_polylog(data &data)
{
	const m::room &room{*data.room};
	m::room::messages it
	{
		room
	};

	ssize_t i(0);
	event::idx idx(0);
	for(; it && i < 10; --it)
	{
		if(apropos(data, it.event_idx()))
		{
			idx = it.event_idx();
			++i;
		}
		else if(i > 0)
			break;
	}

	if(i > 0 && !it && idx)
		it.seek(idx);

	bool ret{false};
	if(i > 0 && idx)
		for(; it && i > -1; ++it, --i)
			ret |= _handle_message(data, it.event_idx());

	return ret;
}

bool
ircd::m::sync::_handle_message(data &data,
                               const event::idx &idx)
{
	bool ret{false};
	if(!apropos(data, idx))
		return ret;

	const event::refs refs{idx};
	refs.for_each(dbs::ref::M_RECEIPT__M_READ, [&data, &ret]
	(const event::idx &idx, const auto &type)
	{
		assert(type == dbs::ref::M_RECEIPT__M_READ);
		static const m::event::fetch::opts fopts
		{
			m::event::keys::include{"content", "sender"}
		};

		const m::event::fetch event
		{
			idx, std::nothrow, fopts
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

	return true;
}
