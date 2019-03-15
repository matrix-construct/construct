// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "app.h"

ircd::mapi::header
IRCD_MODULE
{
	"Application Services",
	ircd::m::app::init,
	ircd::m::app::fini
};

decltype(ircd::m::app::app_room_id)
ircd::m::app::app_room_id
{
	"app", my_host()
};

decltype(ircd::m::app::apps)
ircd::m::app::apps;

void
ircd::m::app::init()
{
	if(!m::exists(app_room_id))
		m::create(app_room_id, m::me, "internal");
}

void
ircd::m::app::fini()
{

}
