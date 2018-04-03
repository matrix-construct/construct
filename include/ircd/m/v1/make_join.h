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
#define HAVE_IRCD_M_V1_MAKE_JOIN_H

namespace ircd::m::v1
{
	struct make_join;
};

struct ircd::m::v1::make_join
:server::request
{
	struct opts;

	operator json::object() const
	{
		return json::object{in.content};
	}

	make_join(const room::id &, const id::user &, const mutable_buffer &, opts);
	make_join(const room::id &, const id::user &, const mutable_buffer &);
	make_join() = default;
};

struct ircd::m::v1::make_join::opts
{
	net::hostport remote;
	m::request request;
	server::out out;
	server::in in;
	const struct server::request::opts *sopts {nullptr};
};
