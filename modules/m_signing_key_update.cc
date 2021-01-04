// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

static void
handle_edu_m_signing_key_update(const m::event &,
                                m::vm::eval &);

mapi::header
IRCD_MODULE
{
	"Matrix Signing Key Update"
};

m::hookfn<m::vm::eval &>
_m_signing_key_update_eval
{
	handle_edu_m_signing_key_update,
	{
		{ "_site",   "vm.effect"             },
		{ "type",    "m.signing_key_update"  },
	}
};

void
handle_edu_m_signing_key_update(const m::event &event,
                                m::vm::eval &eval)
try
{
	if(m::my_host(at<"origin"_>(event)))
		return;

	const json::object &content
	{
		at<"content"_>(event)
	};

	const m::signing_key_update update
	{
		content
	};

	const m::user::id &user_id
	{
		json::get<"user_id"_>(update)
	};

	if(user_id.host() != at<"origin"_>(event))
		return;

	const json::object &msk
	{
		json::get<"master_key"_>(update)
	};

	const m::user::room room
	{
		user_id
	};

	const auto master_id
	{
		msk?
			send(room, user_id, "ircd.device.signing.master", "", msk):
			m::event::id::buf{}
	};

	const json::object &ssk
	{
		json::get<"self_signing_key"_>(update)
	};

	const auto self_id
	{
		ssk?
			send(room, user_id, "ircd.device.signing.self", "", ssk):
			m::event::id::buf{}
	};

	log::info
	{
		m::log, "Signing key update from :%s by %s master:%s self:%s",
		json::get<"origin"_>(event),
		json::get<"user_id"_>(update),
		string_view{master_id},
		string_view{self_id},
	};
}
catch(const ctx::interrupted &e)
{
	throw;
}
catch(const std::exception &e)
{
	log::derror
	{
		m::log, "m.signing_key_update from %s :%s",
		json::get<"origin"_>(event),
		e.what(),
	};
}
