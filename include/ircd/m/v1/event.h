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
#define HAVE_IRCD_M_V1_EVENT_H

namespace ircd::m::v1
{
	struct event;
};

struct ircd::m::v1::event
:server::request
{
	struct opts;

	explicit operator json::object() const
	{
		const json::object object{in.content};
		const json::array pdus{object.at("pdus")};
		return pdus.at(0);
	}

	explicit operator m::event() const
	{
		return json::object{*this};
	}

	event(const m::event::id &, const mutable_buffer &, opts);
	event() = default;
};

struct ircd::m::v1::event::opts
{
	net::hostport remote;
	m::request request;
	server::out out;
	server::in in;
	const struct server::request::opts *sopts {nullptr};
	bool dynamic {false};
};
