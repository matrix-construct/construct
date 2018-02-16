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

	extern struct user me;
	extern struct room my_room;
	extern struct room control;
	extern struct log::log log;

	IRCD_OVERLOAD(generate)
}

namespace ircd::m::self
{
	string_view host();
	bool host(const string_view &);
}

namespace ircd::m
{
	inline string_view my_host()                 { return self::host();        }
	inline bool my_host(const string_view &h)    { return self::host(h);       }
}

namespace ircd
{
	using m::my_host;
}

#include "name.h"
#include "error.h"
#include "id.h"
#include "event.h"
#include "dbs.h"
#include "state.h"
#include "room.h"
#include "user.h"
#include "filter.h"
#include "request.h"
#include "session.h"
#include "v1/v1.h"
#include "vm.h"
#include "keys.h"
#include "txn.h"

struct ircd::m::init
{
	json::object conf;

	void bootstrap();
	void listeners();
	void modules();

	dbs::init _dbs;
	keys::init _keys;

  public:
	init();
	~init() noexcept;
};
