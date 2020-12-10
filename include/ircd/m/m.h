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

// required to disambiguate resolution around some overloads in this ns.
namespace ircd::m
{
	#ifdef __clang__
	using std::operator<;
	using std::operator==;
	using std::operator!=;
	using ircd::json::at;
	using ircd::json::get;
	#endif

	using ircd::operator!;
}

// explicit imports required for GCC or clang
namespace ircd::m
{
	using ircd::hash;
}

/// Matrix Protocol System
namespace ircd::m
{
	struct matrix;
	struct homeserver;

	IRCD_OVERLOAD(generate)

	extern const uint16_t canon_port;
	extern const string_view canon_service;

	extern struct log::log log;
}

namespace ircd
{
	using m::matrix;
}

#include "name.h"
#include "error.h"
#include "id.h"
#include "self.h"
#include "init.h"
#include "event/event.h"
#include "get.h"
#include "query.h"
#include "dbs/dbs.h"
#include "hook.h"
#include "vm/vm.h"
#include "invite_3pid.h"
#include "device.h"
#include "push.h"
#include "createroom.h"
#include "txn.h"
#include "room/room.h"
#include "user/user.h"
#include "users.h"
#include "rooms.h"
#include "rooms_summary.h"
#include "groups.h"
#include "membership.h"
#include "filter.h"
#include "events.h"
#include "node.h"
#include "login.h"
#include "request.h"
#include "fed/fed.h"
#include "keys.h"
#include "edu.h"
#include "presence.h"
#include "typing.h"
#include "receipt.h"
#include "direct_to_device.h"
#include "relates.h"
#include "visible.h"
#include "redacted.h"
#include "feds.h"
#include "app.h"
#include "bridge.h"
#include "sync/sync.h"
#include "fetch.h"
#include "breadcrumbs.h"
#include "media.h"
#include "search.h"
#include "gossip.h"
#include "acquire.h"
#include "burst.h"
#include "resource.h"
#include "homeserver.h"

struct ircd::m::matrix
{
	static const std::vector<string_view> module_names;
	static const std::vector<string_view> module_names_optional;

	std::string module_path
	{
		fs::path_string(fs::path_views
		{
			fs::base::lib, "libircd_matrix"
		})
	};

	ircd::module module
	{
		module_path
	};

	using init_proto = m::homeserver *(const struct m::homeserver::opts *);
	mods::import<init_proto> init
	{
		module, "ircd::m::homeserver::init"
	};

	using fini_proto = void (m::homeserver *);
	mods::import<fini_proto> fini
	{
		module, "ircd::m::homeserver::fini"
	};
};
