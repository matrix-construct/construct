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
#define HAVE_IRCD_M_V1_KEY_H

namespace ircd::m::v1
{
	struct key;
};

struct ircd::m::v1::key
:server::request
{
	struct opts;

	explicit operator json::array() const
	{
		const json::object object{in.content};
		const json::array &server_keys{object.get("server_keys")};
		return server_keys;
	}

	using server_key = std::pair<string_view, string_view>; // server_name, key_id

	key(const vector_view<const server_key> &, const mutable_buffer &, opts);
	key() = default;
};

struct ircd::m::v1::key::opts
{
	net::hostport remote;
	m::request request;
	server::out out;
	server::in in;
	const struct server::request::opts *sopts {nullptr};
	bool dynamic {false};
};
