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
#define HAVE_IRCD_M_FED_GROUPS_H

namespace ircd::m::fed::groups
{
	struct publicised;
};

struct ircd::m::fed::groups::publicised
:request
{
	explicit operator json::object() const
	{
		return json::object
		{
			in.content
		};
	}

	publicised(const string_view &,
	           const vector_view<const id::user> &,
	           const mutable_buffer &,
	           opts);

	publicised() = default;
};
