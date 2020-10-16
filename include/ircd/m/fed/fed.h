// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_FED_H

#include "well_known.h"
#include "request.h"
#include "version.h"
#include "key.h"
#include "query.h"
#include "user.h"
#include "user_keys.h"
#include "make_join.h"
#include "send_join.h"
#include "invite.h"
#include "invite2.h"
#include "event.h"
#include "event_auth.h"
#include "query_auth.h"
#include "state.h"
#include "backfill.h"
#include "frontfill.h"
#include "public_rooms.h"
#include "rooms.h"
#include "send.h"
#include "groups.h"

/// Federation Interface
namespace ircd::m::fed
{
	// Utils
	net::hostport matrix_service(net::hostport remote) noexcept;
	string_view server(const mutable_buffer &out, const string_view &name, const well_known::opts & = {});

	// Observers
	bool errant(const string_view &server_name);
	bool linked(const string_view &server_name);
	bool exists(const string_view &server_name);
	bool avail(const string_view &server_name);

	// Control panel
	bool prelink(const string_view &server_name);
	bool clear_error(const string_view &server_name);
}

inline ircd::net::hostport
ircd::m::fed::matrix_service(net::hostport remote)
noexcept
{
	if(!net::port(remote) && !net::service(remote))
		net::service(remote) = m::canon_service;

	return remote;
}
