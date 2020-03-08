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
#define HAVE_IRCD_M_FED_INVITE2_H

namespace ircd::m::fed
{
	struct invite2;
};

struct ircd::m::fed::invite2
:request
{
	explicit operator json::object() const
	{
		return json::object
		{
			in.content
		};
	}

	invite2(const room::id &,
	        const id::event &,
	        const json::object &,
	        const mutable_buffer &,
	        opts);

	invite2() = default;
};
