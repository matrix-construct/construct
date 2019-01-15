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
#define HAVE_IRCD_M_EVENT_GET_H

namespace ircd::m
{
	// Fetch the value for a single property of an event into the closure
	bool get(std::nothrow_t, const event::idx &, const string_view &key, const event::fetch::view_closure &);
	bool get(std::nothrow_t, const event::id &, const string_view &key, const event::fetch::view_closure &);

	// Throws if event or property in that event not found
	void get(const event::idx &, const string_view &key, const event::fetch::view_closure &);
	void get(const event::id &, const string_view &key, const event::fetch::view_closure &);

	// Copies value into buffer returning view (empty for not found)
	const_buffer get(std::nothrow_t, const event::idx &, const string_view &key, const mutable_buffer &out);
	const_buffer get(std::nothrow_t, const event::id &, const string_view &key, const mutable_buffer &out);

	const_buffer get(const event::idx &, const string_view &key, const mutable_buffer &out);
	const_buffer get(const event::id &, const string_view &key, const mutable_buffer &out);
}
