// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_V1_H

namespace ircd::m::v1
{
	id::event::buf fetch_head(const id::room &room_id, const net::hostport &remote, const id::user &);
	id::event::buf fetch_head(const id::room &room_id, const net::hostport &remote);
}

#include "version.h"
#include "query.h"
#include "user.h"
#include "make_join.h"
#include "send_join.h"
#include "invite.h"
#include "event.h"
#include "event_auth.h"
#include "state.h"
#include "backfill.h"
#include "public_rooms.h"
#include "send.h"
#include "groups.h"
