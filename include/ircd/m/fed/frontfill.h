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
#define HAVE_IRCD_M_FED_FRONTFILL_H

namespace ircd::m::fed
{
	struct frontfill;
};

struct ircd::m::fed::frontfill
:request
{
	struct opts;
	using span = std::pair<m::event::id, m::event::id>;
	using vector = vector_view<const m::event::id>;
	using ranges = std::pair<vector, vector>;

	static const_buffer make_content(const mutable_buffer &, const ranges &, const opts &);

	explicit operator json::array() const
	{
		const json::object content
		{
			in.content
		};

		return content["events"];
	}

	frontfill(const room::id &,
	          const ranges &,
	          const mutable_buffer &,
	          opts);

	frontfill(const room::id &,
	          const span &,
	          const mutable_buffer &,
	          opts);

	frontfill() = default;
};

struct ircd::m::fed::frontfill::opts
:request::opts
{
	size_t limit {64};
	uint64_t min_depth {0};
};
