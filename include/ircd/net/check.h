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
#define HAVE_IRCD_NET_CHECK_H

namespace ircd::net
{
	// !!!
	// NOTE: only ready::ERROR is supported for this call at this time.
	// !!!

	// Check if the socket is ready, similar to wait(), but without blocking.
	// Returns true if the socket is ready for that type; false if it is not.
	// For ready::ERROR, returning true means the socket has a pending error.
	error_code check(std::nothrow_t, socket &, const ready & = ready::ERROR) noexcept;

	// Throws any error.
	void check(socket &, const ready & = ready::ERROR);
}
