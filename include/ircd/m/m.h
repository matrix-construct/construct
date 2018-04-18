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

/// Matrix Protocol System
namespace ircd::m
{
	struct init;

	extern struct log::log log;
	extern std::list<ircd::net::listener> listeners;

	using ircd::hash;
	IRCD_OVERLOAD(generate)
}

namespace ircd::m::self
{
	string_view host();
	bool host(const string_view &);
}

namespace ircd::m::vm
{
	struct opts;
}

namespace ircd::m
{
	extern struct user me;
	extern struct room my_room;
	extern struct node my_node;
	extern struct room control;

	inline string_view my_host()                 { return self::host();        }
	inline bool my_host(const string_view &h)    { return self::host(h);       }
}

namespace ircd
{
	using m::my_host;
}

#include "name.h"
#include "error.h"
#include "import.h"
#include "id.h"
#include "event.h"
#include "dbs.h"
#include "state.h"
#include "vm.h"
#include "room.h"
#include "user.h"
#include "rooms.h"
#include "events.h"
#include "node.h"
#include "login.h"
#include "register.h"
#include "invite_3pid.h"
#include "createroom.h"
#include "filter.h"
#include "request.h"
#include "v1/v1.h"
#include "keys.h"
#include "edu.h"
#include "presence.h"
#include "typing.h"
#include "receipt.h"
#include "txn.h"
#include "hook.h"

struct ircd::m::init
{
	struct modules;
	struct listeners;

	json::object config;
	keys::init _keys;
	dbs::init _dbs;
	vm::init _vm;
	std::unique_ptr<modules> modules;
	std::unique_ptr<listeners> listeners;

	static void bootstrap();

  public:
	init();
	~init() noexcept;
};
