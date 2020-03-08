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
#define HAVE_IRCD_M_FED_BACKFILL_H

namespace ircd::m::fed
{
	struct backfill;
};

struct ircd::m::fed::backfill
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

	backfill(const room::id &,
	         const mutable_buffer &,
	         opts);

	backfill() = default;
};

struct ircd::m::fed::backfill::opts
:request::opts
{
	string_view event_id;
	size_t limit {64};
};
