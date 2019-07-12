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
#define HAVE_IRCD_M_V1_QUERY_H

namespace ircd::m::v1
{
	struct query;
};

struct ircd::m::v1::query
:server::request
{
	struct opts;
	struct profile;
	struct directory;
	struct user_devices;
	struct client_keys;

	explicit operator json::object() const
	{
		const json::object object{in.content};
		return object;
	}

	query(const string_view &type, const string_view &args, const mutable_buffer &, opts);
	query() = default;
};

struct ircd::m::v1::query::opts
{
	net::hostport remote;
	m::request request;
	server::out out;
	server::in in;
	const struct server::request::opts *sopts {nullptr};
	bool dynamic {false};

	opts(const net::hostport &remote)
	:remote{remote}
	{}

	opts() = default;
};

struct ircd::m::v1::query::profile
:query
{
	profile(const id::user &user_id, const string_view &field, const mutable_buffer &, opts);
	profile(const id::user &user_id, const mutable_buffer &, opts);
};

struct ircd::m::v1::query::directory
:query
{
	directory(const id::room_alias &room_alias, const mutable_buffer &, opts);
};
