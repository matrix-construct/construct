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
#define HAVE_IRCD_M_V1_EVENT_AUTH_H

namespace ircd::m::v1
{
	struct event_auth;
};

struct ircd::m::v1::event_auth
:server::request
{
	struct opts;

	explicit operator json::array() const
	{
		const json::object object{in.content};
		return object.at("auth_chain");
	}

	event_auth(const m::room::id &, const m::event::id &, const mutable_buffer &, opts);
	event_auth() = default;
};

struct ircd::m::v1::event_auth::opts
{
	net::hostport remote;
	m::request request;
	server::out out;
	server::in in;
	const struct server::request::opts *sopts {nullptr};
	bool dynamic {true};
};
