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
}

#include "event/event.h"
#include "event/prev.h"
#include "event/refs.h"
#include "event/index.h"
#include "event/fetch.h"
#include "event/get.h"
#include "event/cached.h"
#include "event/prefetch.h"
#include "event/conforms.h"
#include "event/pretty.h"
