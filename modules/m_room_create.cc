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
	"Matrix m.room.create"
};

static void
_can_create_room(const m::event &event)
{

}

const m::hook
_can_create_room_hookfn
{
	_can_create_room,
	{
		{ "_site",    "vm.commit"     },
		{ "type",     "m.room.create" },
	}
};

static void
_created_room(const m::event &event)
{

}

const m::hook
_created_room_hookfn
{
	_created_room,
	{
		{ "_site",    "vm.notify"     },
		{ "type",     "m.room.create" },
	}
};
