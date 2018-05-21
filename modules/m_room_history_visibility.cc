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
	"Matrix m.room.history_visibility"
};

static void
_changed_visibility(const m::event &event)
{

}

const m::hook
_changed_visibility_hookfn
{
	_changed_visibility,
	{
		{ "_site",    "vm.notify"                  },
		{ "type",     "m.room.history_visibility"  },
	}
};

static void
_event_visible(const m::event &event)
{

}

const m::hook
_event_visible_hookfn
{
	_event_visible,
	{
		{ "_site",    "m.event.visible"            },
	}
};
