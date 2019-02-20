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
	// event/event.h
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

	// Topological comparison
	bool before(const event &a, const event &b);

	id::event make_id(const event &, id::event::buf &buf, const const_buffer &hash);
	id::event make_id(const event &, id::event::buf &buf);

	json::object hashes(const mutable_buffer &, const event &);
	event signatures(const mutable_buffer &, const m::event &);
	event essential(event, const mutable_buffer &content);

	bool verify_sha256b64(const event &, const string_view &);
	bool verify_hash(const event &, const sha256::buf &);
	bool verify_hash(const event &);

	bool verify(const event &, const ed25519::pk &, const ed25519::sig &sig);
	bool verify(const event &, const ed25519::pk &, const string_view &origin, const string_view &pkid);
	bool verify(const event &, const string_view &origin, const string_view &pkid); // io/yield
	bool verify(const event &, const string_view &origin); // io/yield
	bool verify(const event &); // io/yield

	sha256::buf hash(const event &);
	ed25519::sig sign(const event &, const ed25519::sk &);
	ed25519::sig sign(const event &);
}

#include "event/event.h"
#include "event/prev.h"
#include "event/refs.h"
#include "event/auth.h"
#include "event/index.h"
#include "event/event_id.h"
#include "event/fetch.h"
#include "event/get.h"
#include "event/cached.h"
#include "event/prefetch.h"
#include "event/conforms.h"
#include "event/pretty.h"
