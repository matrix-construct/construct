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
#define HAVE_IRCD_M_V1_PUBLIC_ROOMS_H

namespace ircd::m::v1
{
	struct public_rooms;
};

struct ircd::m::v1::public_rooms
:server::request
{
	struct opts;

	explicit operator json::object() const
	{
		return json::object{in.content};
	}

	public_rooms(const net::hostport &, const mutable_buffer &, opts);
	public_rooms() = default;
};

struct ircd::m::v1::public_rooms::opts
{
	net::hostport remote;
	size_t limit {128};
	string_view since;
	string_view third_party_instance_id;
	bool include_all_networks {true};
	m::request request;
	server::out out;
	server::in in;
	const struct server::request::opts *sopts {nullptr};
	bool dynamic {true};
};
