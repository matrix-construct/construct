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
	"Matrix Typing"
};

static void _handle_edu_m_typing(const m::event &, const m::edu::m_typing &edu);
static void handle_edu_m_typing(const m::event &);

const m::hook
_m_typing_eval
{
	handle_edu_m_typing,
	{
		{ "_site",   "vm.eval"     },
		{ "type",    "m.typing"    },
	}
};

void
handle_edu_m_typing(const m::event &event)
{
	const json::object &content
	{
		at<"content"_>(event)
	};

	_handle_edu_m_typing(event, content);
}

void
_handle_edu_m_typing(const m::event &event,
                     const m::edu::m_typing &edu)
{
	const auto &room_id
	{
		json::get<"room_id"_>(edu)
	};

	if(!room_id)
		return;

	const m::user::id &user_id
	{
		at<"user_id"_>(edu)
	};

	if(user_id.host() != at<"origin"_>(event))
	{
		log::dwarning
		{
			"Ignoring %s from %s for user %s",
			at<"type"_>(event),
			at<"origin"_>(event),
			string_view{user_id}
		};

		return;
	}

	log::info
	{
		"%s %s %s typing in %s",
		at<"origin"_>(event),
		string_view{user_id},
		json::get<"typing"_>(edu)? "started"_sv : "stopped"_sv,
		string_view{room_id}
	};

	m::event typing{event};
	json::get<"room_id"_>(typing) = room_id;
	json::get<"type"_>(typing) = "m.typing";

	char buf[512];
	const json::value user_ids[]
	{
		{ user_id }
	};

	json::get<"content"_>(typing) = json::stringify(mutable_buffer{buf}, json::members
	{
		{ "user_ids", { user_ids, size_t(bool(json::get<"typing"_>(edu))) } }
	});

	m::vm::opts vmopts;
	vmopts.non_conform.set(m::event::conforms::INVALID_OR_MISSING_EVENT_ID);
	vmopts.non_conform.set(m::event::conforms::INVALID_OR_MISSING_ROOM_ID);
	vmopts.non_conform.set(m::event::conforms::INVALID_OR_MISSING_SENDER_ID);
	vmopts.non_conform.set(m::event::conforms::MISSING_ORIGIN_SIGNATURE);
	vmopts.non_conform.set(m::event::conforms::MISSING_SIGNATURES);
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_EVENTS);
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	vmopts.non_conform.set(m::event::conforms::DEPTH_ZERO);
	m::vm::eval eval
	{
		typing, vmopts
	};
}
