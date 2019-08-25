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
	static void room_name_length_conform(const event &, vm::eval &);
	extern hookfn<vm::eval &> room_name_length_conform_hookfn;
	extern const size_t room_name_length_max;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix m.room.name"
};

/// Spec sez in c2s 13.2.1.3 m.room.name MUST NOT exceed 255 bytes.
decltype(ircd::m::room_name_length_max)
ircd::m::room_name_length_max
{
	255
};

decltype(ircd::m::room_name_length_conform_hookfn)
ircd::m::room_name_length_conform_hookfn
{
	room_name_length_conform,
	{
		{ "_site",      "vm.conform"   },
		{ "type",       "m.room.name"  },
		{ "state_key",  ""             },
	}
};

void
ircd::m::room_name_length_conform(const m::event &event,
                                  m::vm::eval &)
{
	assert(at<"type"_>(event) == "m.room.name");

	const auto &content
	{
		json::get<"content"_>(event)
	};

	const json::string &name
	{
		content.get("name")
	};

	if(size(name) > room_name_length_max)
		throw vm::error
		{
			vm::fault::INVALID,
			"m.room.name content.name is %zu characters longer than the %zu allowed.",
			size(name) - room_name_length_max,
			room_name_length_max,
		};
}
