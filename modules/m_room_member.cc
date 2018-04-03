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
	"Matrix m.room.member"
};

static void
_can_join_room(const m::event &event)
{

}

const m::hook
_can_join_room_hookfn
{
	{
		{ "_site",          "vm.commit"     },
		{ "type",           "m.room.member" },
		{ "membership",     "join"          },
	},
	_can_join_room
};

static void
_join_room(const m::event &event)
{

}

const m::hook
_join_room_hookfn
{
	{
		{ "_site",          "vm.notify"     },
		{ "type",           "m.room.member" },
		{ "membership",     "join"          },
	},
	_join_room
};
