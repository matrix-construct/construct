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

static void handle_m_receipt_m_read(const m::room::id &, const m::user::id &, const m::edu::m_receipt::m_read &);
static void handle_m_receipt_m_read(const m::room::id &, const json::object &);
static void handle_m_receipt(const m::room::id &, const json::object &);
static void handle_edu_m_receipt(const m::event &);
extern "C" m::event::id::buf receipt_m_read(const m::room::id &, const m::user::id &, const m::event::id &, const time_t &);

m::event::id::buf
receipt_m_read(const m::room::id &room_id,
               const m::user::id &user_id,
               const m::event::id &event_id,
               const time_t &ms)
{
	const json::value event_ids[]
	{
		{ event_id }
	};

	const json::members m_read
	{
		{ "data",
		{
			{ "ts", ms }
		}},
		{ "event_ids", { event_ids, 1 } },
	};

	json::iov event, content;
	const json::iov::push push[]
	{
		{ event,    { "type",     "m.receipt" } },
		{ event,    { "room_id",   room_id    } },
		{ content,  { room_id,
		{
			{ "m.read",
			{
				{ user_id, m_read }
			}}
		}}}
	};

	m::vm::opts opts;
	opts.hash = false;
	opts.sign = false;
	opts.event_id = false;
	opts.origin = true;
	opts.origin_server_ts = false;
	opts.conforming = false;

	return m::vm::commit(event, content, opts);
}

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

		log::warning
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
}
