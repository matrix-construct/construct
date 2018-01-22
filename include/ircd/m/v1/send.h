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
#define HAVE_IRCD_M_V1_SEND_H

namespace ircd::m::v1
{
	struct send;
};

struct ircd::m::v1::send
:server::request
{
	struct opts;

	send(const string_view &txnid, const const_buffer &content, const mutable_buffer &head, opts);
};

struct ircd::m::v1::send::opts
{
	net::hostport remote;
	m::request request;
	server::out out;
	server::in in;
	const struct server::request::opts *sopts {nullptr};
};
