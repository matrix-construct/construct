// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma GCC visibility push(hidden)
namespace ircd::m
{
	string_view make_state_key(const mutable_buffer &, const string_view &, const string_view &, const event::idx &);
	std::tuple<string_view, string_view, string_view> unmake_state_key(const string_view &);
}
#pragma GCC visibility pop
