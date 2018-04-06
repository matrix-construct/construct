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
#define HAVE_IRCD_M_V1_GROUPS_H

namespace ircd::m::v1::groups
{
	struct publicised;
};

struct ircd::m::v1::groups::publicised
:server::request
{
	struct opts;

	operator json::object() const
	{
		return json::object{in.content};
	}

	publicised(const id::node &, const vector_view<const id::user> &, const mutable_buffer &, opts);
	publicised() = default;
};

struct ircd::m::v1::groups::publicised::opts
{
	net::hostport remote;
	m::request request;
	server::out out;
	server::in in;
	const struct server::request::opts *sopts {nullptr};
	bool dynamic {false};
};
