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
#define HAVE_IRCD_BASE64_H

namespace ircd
{
	// Binary -> Base64 conversion suite
	string_view b64encode(const mutable_buffer &out, const const_raw_buffer &in);
	std::string b64encode(const const_raw_buffer &in);

	// Binary -> Base64 conversion without padding
	string_view b64encode_unpadded(const mutable_buffer &out, const const_raw_buffer &in);
	std::string b64encode_unpadded(const const_raw_buffer &in);

	// Base64 -> Binary conversion (padded or unpadded)
	const_raw_buffer b64decode(const mutable_raw_buffer &out, const string_view &in);
	std::string b64decode(const string_view &in);
}
