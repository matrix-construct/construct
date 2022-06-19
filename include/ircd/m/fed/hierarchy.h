// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_FED_HIERARCHY_H

namespace ircd::m::fed
{
	struct hierarchy;
};

struct ircd::m::fed::hierarchy
:request
{
	struct opts;

	explicit operator json::object() const
	{
		return json::object
		{
			in.content
		};
	}

	hierarchy(const room::id &,
	          const mutable_buffer &,
	          opts);

	hierarchy() = default;
};

struct ircd::m::fed::hierarchy::opts
:request::opts
{
	bool suggested_only {false};
};
