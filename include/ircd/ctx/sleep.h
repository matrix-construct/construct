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
#define HAVE_IRCD_CTX_SLEEP_H

namespace ircd::ctx { inline namespace this_ctx
{
	// Ignores notes. Throws if interrupted.
	void sleep_until(const system_point &tp);
	template<class duration> void sleep(const duration &);
	void sleep(const int &secs);
}}

/// This overload matches ::sleep() and acts as a drop-in for ircd contexts.
/// interruption point.
inline void
ircd::ctx::this_ctx::sleep(const int &secs)
{
	sleep(seconds(secs));
}

/// Yield the context for a period of time and ignore notifications. sleep()
/// is like wait() but it only returns after the timeout and not because of a
/// note.
/// interruption point.
template<class duration>
void
ircd::ctx::this_ctx::sleep(const duration &d)
{
	sleep_until(system_clock::now() + d);
}
