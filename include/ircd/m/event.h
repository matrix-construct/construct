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
#define HAVE_IRCD_M_EVENT_H

namespace ircd::m
{
	struct event;

	// General util
	bool my(const id::event &);
	bool my(const event &);
	size_t degree(const event &);
	string_view membership(const event &);
	bool check_size(std::nothrow_t, const event &);
	void check_size(const event &);

	// [GET]
	bool exists(const id::event &);
	bool exists(const id::event &, const bool &good);
	bool cached(const id::event &);
	bool good(const id::event &);
	bool bad(const id::event &);

	// Equality tests the event_id only! know this.
	bool operator==(const event &a, const event &b);

	// Depth comparison; expect unstable sorting.
	bool operator<(const event &, const event &);
	bool operator>(const event &, const event &);
	bool operator<=(const event &, const event &);
	bool operator>=(const event &, const event &);

	// Topological
	bool before(const event &a, const event &b); // A directly referenced by B

	id::event make_id(const event &, id::event::buf &buf, const const_buffer &hash);
	id::event make_id(const event &, id::event::buf &buf);

	// Informational pretty string condensed to single line.
	std::ostream &pretty_oneline(std::ostream &, const event &, const bool &content_keys = true);
	std::string pretty_oneline(const event &, const bool &content_keys = true);

	// Informational pretty string on multiple lines.
	std::ostream &pretty(std::ostream &, const event &);
	std::string pretty(const event &);

	// Informational content-oriented
	std::ostream &pretty_msgline(std::ostream &, const event &);
	std::string pretty_msgline(const event &);
}

#include "event/event.h"
#include "event/prev.h"
#include "event/fetch.h"
#include "event/conforms.h"
