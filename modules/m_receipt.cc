// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Matrix Receipts"
};

static void handle_m_receipt_m_read(const m::room::id &, const m::user::id &, const m::event::id &, const time_t &);
static void handle_m_receipt_m_read(const m::room::id &, const m::user::id &, const m::edu::m_receipt::m_read &);
static void handle_m_receipt_m_read(const m::room::id &, const json::object &);
static void handle_m_receipt(const m::room::id &, const json::object &);
static void handle_edu_m_receipt(const m::event &);

const m::hook
_m_receipt_eval
{
	handle_edu_m_receipt,
	{
		{ "_site",   "vm.eval"    },
		{ "type",    "m.receipt"  },
	}
};

void
handle_edu_m_receipt(const m::event &event)
{
	const json::object &content
	{
		at<"content"_>(event)
	};

	for(const auto &member : content)
	{
		const m::room::id &room_id
		{
			unquote(member.first)
		};

		handle_m_receipt(room_id, member.second);
	}
}

void
handle_m_receipt(const m::room::id &room_id,
                 const json::object &content)
{
	for(const auto &member : content)
	{
		const string_view &type
		{
			unquote(member.first)
		};

		if(type == "m.read")
		{
			handle_m_receipt_m_read(room_id, member.second);
			continue;
		}

		log::dwarning
		{
			"Unhandled m.receipt type '%s' to room '%s'",
			type,
			string_view{room_id}
		};
	}
}

void
handle_m_receipt_m_read(const m::room::id &room_id,
                        const json::object &content)
{
	for(const auto &member : content)
	{
		const m::user::id user_id
		{
			unquote(member.first)
		};

		const json::object &content
		{
			member.second
		};

		handle_m_receipt_m_read(room_id, user_id, content);
	}
}

void
handle_m_receipt_m_read(const m::room::id &room_id,
                        const m::user::id &user_id,
                        const m::edu::m_receipt::m_read &receipt)
{
	const json::array &event_ids
	{
		json::get<"event_ids"_>(receipt)
	};

	const json::object &data
	{
		json::get<"data"_>(receipt)
	};

	const time_t &ts
	{
		data.get<time_t>("ts")
	};

	for(const string_view &event_id : event_ids)
		handle_m_receipt_m_read(room_id, user_id, unquote(event_id), ts);
}

void
handle_m_receipt_m_read(const m::room::id &room_id,
                        const m::user::id &user_id,
                        const m::event::id &event_id,
                        const time_t &ts)
{
}
