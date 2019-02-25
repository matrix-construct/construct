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
#define HAVE_IRCD_M_V1_BACKFILL_H

namespace ircd::m::v1
{
	struct backfill;
};

struct ircd::m::v1::backfill
:server::request
{
	struct opts;

	explicit operator json::object() const
	{
		return json::object{in.content};
	}

	backfill(const room::id &, const mutable_buffer &, opts);
	backfill() = default;
};

struct ircd::m::v1::backfill::opts
{
	net::hostport remote;
	string_view event_id;
	size_t limit {64};
	m::request request;
	server::out out;
	server::in in;
	const struct server::request::opts *sopts {nullptr};
	bool dynamic {true};
};
