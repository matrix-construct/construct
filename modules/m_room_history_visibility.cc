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

namespace ircd::m
{
	extern hook::site visible_hook;
}

decltype(ircd::m::visible_hook)
ircd::m::visible_hook
{
	{ "name", "m.event.visible" }
};

extern "C" bool
_visible__user(const m::room &room,
               const m::user &user,
               const m::event *const &event)
{
	return true;
}

extern "C" bool
_visible__node(const m::room &room,
               const m::node &node,
               const m::event *const &event)
{
	return true;
}

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
