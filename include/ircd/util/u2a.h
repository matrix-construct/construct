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
#define HAVE_IRCD_UTIL_U2A_H

namespace ircd::util
{
	// Binary <-> Hex conversion suite
	const_buffer a2u(const mutable_buffer &out, const const_buffer &in);
	string_view u2a(const mutable_buffer &out, const const_buffer &in);
	std::string u2a(const const_buffer &in);
}
