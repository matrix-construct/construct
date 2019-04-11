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
#define HAVE_IRCD_M_FETCH_H

/// Event Fetcher (remote). This is probably not the interface you want
/// because there is no ordinary reason for developers fetch a remote event
/// directly. This is an interface to the low-level fetch system.
///
namespace ircd::m::fetch
{
	struct request;

	void state_ids(const room &, const net::hostport &);
	void state_ids(const room &);

	extern log::log log;
}
