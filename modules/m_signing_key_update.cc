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

	if(!exists(user_id))
	{
		log::derror
		{
			m::log, "Refusing signing key update for unknown %s",
			string_view{user_id},
		};

		return;
	}

	const m::user::keys keys
	{
		user_id
	};

	keys.update(update);

	log::info
	{
		m::log, "Signing key update from '%s' for %s",
		json::get<"origin"_>(event),
		json::get<"user_id"_>(update),
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
		m::log, "m.signing_key_update from '%s' :%s",
		json::get<"origin"_>(event),
		e.what(),
	};
}
