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
#define HAVE_IRCD_M_ROOMS_H

/// Convenience iterface for iterations of rooms
///
namespace ircd::m::rooms
{
	void for_each(const room::id::closure_bool &);
	void for_each(const room::id::closure &);
	void for_each(const room::closure_bool &);
	void for_each(const room::closure &);
}
