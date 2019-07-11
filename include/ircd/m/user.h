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
#define HAVE_IRCD_M_USER_H

namespace ircd::m
{
	struct user;

	bool my(const user &);
	bool exists(const user &);
	bool exists(const id::user &);

	user create(const id::user &, const json::members &args = {});
}

#include "user/user.h"
#include "user/room.h"
#include "user/rooms.h"
#include "user/mitsein.h"
#include "user/events.h"
#include "user/profile.h"
#include "user/account_data.h"
#include "user/room_account_data.h"
#include "user/room_tags.h"
#include "user/filter.h"
#include "user/ignores.h"
#include "user/highlight.h"
