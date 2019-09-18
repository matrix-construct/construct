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
#define HAVE_IRCD_M_MEMBERSHIP_H

namespace ircd::m
{
	// Extract membership string from event data.
	string_view membership(const event &);

	// Query and copy membership string to buffer. Note that the event type
	// is not checked here, only content.membership is sought.
	string_view membership(const mutable_buffer &out, const event::idx &);

	// Query and copy membership string to buffer; queries room state. (also room.h)
	string_view membership(const mutable_buffer &out, const room &, const id::user &);

	// Query and compare membership string to argument string. Returns true on
	// equal; false on not equal; false on not found. Note in addition to this
	// we allow the user to pass an empty membership string which will test for
	// non-membership as well and return true.
	bool membership(const event::idx &, const string_view &);

	// Query and compare membership string; queries room state. (also room.h)
	// also see overload notes.
	bool membership(const room &, const id::user &, const string_view &);
}
