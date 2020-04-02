// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

static void
handle_edu_m_device_list_update(const m::event &,
                                m::vm::eval &);

mapi::header
IRCD_MODULE
{
	"Matrix Device List Update"
};

m::hookfn<m::vm::eval &>
_m_device_list_update_eval
{
	handle_edu_m_device_list_update,
	{
		{ "_site",   "vm.effect"             },
		{ "type",    "m.device_list_update"  },
	}
};

void
handle_edu_m_device_list_update(const m::event &event,
                                m::vm::eval &eval)
try
{
	if(m::my_host(at<"origin"_>(event)))
		return;

	const json::object &content
	{
		at<"content"_>(event)
	};

	const m::device_list_update update
	{
		content
	};

	const m::user::id &user_id
	{
		json::get<"user_id"_>(update)
	};

	if(user_id.host() != at<"origin"_>(event))
		return;

	const bool updated
	{
		m::user::devices::update(update)
	};

	if(!updated)
		return;

	log::info
	{
		m::log, "Device list update from :%s by %s for '%s' sid:%lu %s",
		json::get<"origin"_>(event),
		json::get<"user_id"_>(update),
		json::get<"device_id"_>(update),
		json::get<"stream_id"_>(update),
		json::get<"deleted"_>(update)?
			"[deleted]"_sv:
			string_view{}
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
		m::log, "m.device_list_update from %s :%s",
		json::get<"origin"_>(event),
		e.what(),
	};
}
