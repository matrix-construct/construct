// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_SYNC_SYNC_H

/// Matrix Client Synchronization.
///
/// This header and contents of this namespace within the central matrix
/// library exist to provide support for the actual sync functionality which
/// is distributed within modules under `modules/client/sync/`. This central
/// interface simplifies the sharing of common structures and functions among
/// the modules.
///
/// The primary module which drives client sync is `modules/client/sync.cc` and
/// the functionality for each aspect of a /sync request and response is
/// provided by the tree of modules.
namespace ircd::m::sync
{
	extern ctx::pool pool;
	extern log::log log;
}

#include "since.h"
#include "item.h"
#include "stats.h"
#include "args.h"
#include "data.h"
