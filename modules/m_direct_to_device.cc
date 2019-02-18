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
	"Matrix Direct To Device"
};

static void
handle_m_direct_to_device(m::vm::eval &,
                          const m::direct_to_device &,
                          const m::user::id &user_id,
                          const string_view &device_id,
                          const json::object &message);

static void
handle_edu_m_direct_to_device(const m::event &,
                              m::vm::eval &);

const m::hookfn<m::vm::eval &>
_m_direct_to_device_eval
{
	handle_edu_m_direct_to_device,
	{
		{ "_site",   "vm.eval"             },
		{ "type",    "m.direct_to_device"  },
	}
};

void
handle_edu_m_direct_to_device(const m::event &event,
                              m::vm::eval &eval)
try
{
	if(m::my_host(at<"origin"_>(event)))
		return;

	const json::object &content
	{
		at<"content"_>(event)
	};

	const m::direct_to_device edu
	{
		content
	};

	const m::user::id &sender
	{
		at<"sender"_>(edu)
	};

	if(sender.host() != at<"origin"_>(event))
		throw m::ACCESS_DENIED
		{
			"Cannot send indirect direct-to-device messages; "
			"Sender's host must match EDU's origin."
		};

	for(const auto &user_messages : at<"messages"_>(edu))
	{
		const m::user::id user_id
		{
			user_messages.first
		};

		const json::object &device_messages
		{
			user_messages.second
		};

		for(const auto &device_message : device_messages)
		{
			const string_view &device_id
			{
				device_message.first
			};

			const json::object &message_body
			{
				device_message.second
			};

			handle_m_direct_to_device(eval, edu, user_id, device_id, message_body);
		}
	}
}
catch(const std::exception &e)
{
	log::derror
	{
		m::log, "m.direct_to_device from %s :%s",
		json::get<"origin"_>(event),
		e.what(),
	};
}

static void
handle_m_direct_to_device(m::vm::eval &eval,
                          const m::direct_to_device &edu,
                          const m::user::id &user_id,
                          const string_view &device_id,
                          const json::object &message)
{
	const m::user::room user_room
	{
		user_id
	};

	log::info
	{
		m::log, "%s sent '%s' to %s device '%s' (%zu bytes)",
		at<"sender"_>(edu),
		at<"type"_>(edu),
		string_view{user_id},
		device_id,
		size(string_view{message})
	};
}
