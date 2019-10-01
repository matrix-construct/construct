// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	extern hookfn<vm::eval &> _access_token_delete_hook;
	static void _access_token_delete(const m::event &event, m::vm::eval &eval);
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix device library; modular components."
};

/// Deletes the access_token associated with a device when the device
/// (specifically the access_token_id property of that device) is deleted.
decltype(ircd::m::_access_token_delete_hook)
ircd::m::_access_token_delete_hook
{
	_access_token_delete,
	{
		{ "_site",   "vm.effect"         },
		{ "type",    "m.room.redaction"  },
		{ "origin",  my_host()           },
	}
};

void
ircd::m::_access_token_delete(const m::event &event,
                              m::vm::eval &eval)
{
	const auto &target(json::get<"redacts"_>(event));
	if(!target)
		return;

	char buf[std::max(m::event::TYPE_MAX_SIZE, m::id::MAX_SIZE)];
	if(m::get(std::nothrow, target, "type", buf) != "ircd.device.access_token_id"_sv)
		return;

	if(m::get(std::nothrow, target, "sender", buf) != at<"sender"_>(event))
		return;

	m::get(std::nothrow, target, "content", [&event, &target, &buf]
	(const json::object &content)
	{
		const event::id &token_event_id
		{
			unquote(content.at(""))
		};

		const m::room::id::buf tokens_room
		{
			"tokens", origin(m::my())
		};

		m::redact(tokens_room, at<"sender"_>(event), token_event_id, "device deleted");
	});
};
