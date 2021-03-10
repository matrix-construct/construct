// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_GPT_GENERATE_H

namespace ircd::gpt
{
	vector_view<u16>
	generate(const vector_view<u16> &out,
	         const vector_view<const u16> &in,
	         const opts * = &default_opts,
	         task * = nullptr);

	string_view
	generate(const mutable_buffer &out,
	         const string_view &in,
	         const opts * = &default_opts,
	         task * = nullptr);
}
