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
#define HAVE_IRCD_M_H

// required for clang to consider string_view operators inherited by
// m::id among other import disambiguations.
#ifdef __clang__
namespace ircd::m
{
    using std::operator==;
    using std::operator!=;
    using std::operator<;
    using ircd::operator!;
    using ircd::json::at;
    using ircd::json::get;
}
#endif

// explicit imports required for GCC or clang
namespace ircd::m
{
	using ircd::hash;
}

/// Matrix Protocol System
namespace ircd::m
{
	struct init;

	IRCD_OVERLOAD(generate)

	extern struct log::log log;
}

#include "name.h"
#include "error.h"
#include "self.h"
#include "id.h"
#include "event/event.h"
#include "get.h"
#include "query.h"
#include "dbs/dbs.h"
#include "vm.h"
#include "invite_3pid.h"
#include "createroom.h"
#include "room/room.h"
#include "user/user.h"
#include "users.h"
#include "rooms.h"
#include "filter.h"
#include "events.h"
#include "node.h"
#include "login.h"
#include "register.h"
#include "device.h"
#include "request.h"
#include "fed/fed.h"
#include "keys.h"
#include "edu.h"
#include "presence.h"
#include "typing.h"
#include "receipt.h"
#include "direct_to_device.h"
#include "txn.h"
#include "hook.h"
#include "visible.h"
#include "feds.h"
#include "app.h"
#include "sync.h"
#include "fetch.h"
#include "breadcrumb_rooms.h"

struct ircd::m::init
{
	struct modules;

	self::init _self;
	dbs::init _dbs;
	std::unique_ptr<modules> _modules;

	static void bootstrap();
	void close();

  public:
	init(const string_view &origin, const string_view &hostname);
	~init() noexcept;
};

struct ircd::m::init::modules
{
	void fini_imports() noexcept;
	void init_imports();
	void init_keys();

  public:
	modules();
	~modules() noexcept;
};
