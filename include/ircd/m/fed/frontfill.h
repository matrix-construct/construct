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
#define HAVE_IRCD_M_V1_FRONTFILL_H

namespace ircd::m::v1
{
	struct frontfill;
};

struct ircd::m::v1::frontfill
:server::request
{
	struct opts;
	using span = std::pair<m::event::id, m::event::id>;
	using vector = vector_view<const m::event::id>;
	using ranges = std::pair<vector, vector>;

    static const_buffer make_content(const mutable_buffer &, const ranges &, const opts &);

	explicit operator json::array() const
	{
		const json::object content{in.content};
		return content.get("events");
	}

	frontfill(const room::id &, const ranges &, const mutable_buffer &, opts);
	frontfill(const room::id &, const span &, const mutable_buffer &, opts);
	frontfill() = default;
};

struct ircd::m::v1::frontfill::opts
{
	net::hostport remote;
	size_t limit {64};
	uint64_t min_depth {0};
	m::request request;
	server::out out;
	server::in in;
	const struct server::request::opts *sopts {nullptr};
	bool dynamic {true};
};
