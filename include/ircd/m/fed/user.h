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
#define HAVE_IRCD_M_V1_USER_H

namespace ircd::m::v1::user
{
	struct opts;
	struct devices;
}

struct ircd::m::v1::user::devices
:server::request
{
	using opts = v1::user::opts;

	explicit operator json::object() const
	{
		const json::object object{in.content};
		return object;
	}

	devices(const id::user &user_id, const mutable_buffer &, opts);
	devices() = default;
};

struct ircd::m::v1::user::opts
{
	net::hostport remote;
	m::request request;
	server::out out;
	server::in in;
	const struct server::request::opts *sopts {nullptr};
	bool dynamic {true};

	opts(const net::hostport &remote)
	:remote{remote}
	{}

	opts() = default;
};
