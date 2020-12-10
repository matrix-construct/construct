// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_RESOURCE_H

namespace ircd::m
{
	struct resource;
}

/// Extension of the ircd::resource framework for matrix resource handlers.
struct ircd::m::resource
:ircd::resource
{
	struct method;
	struct request;

	static log::log log;

	using ircd::resource::resource;
};

struct ircd::m::resource::method
:ircd::resource::method
{
	using handler = std::function<response (client &, request &)>;

	handler function;

	response handle(client &, ircd::resource::request &);

	method(m::resource &, const string_view &name, handler, struct opts = {});
	~method() noexcept;
};

struct ircd::m::resource::request
:ircd::resource::request
{
	template<class> struct object;

	pair<string_view> authorization;   // proffering any
	string_view access_token;          // proffering user
	m::request::x_matrix x_matrix;     // proferring server
	pair<string_view> version;         // enumeration

	string_view node_id;               // authenticated server
	m::user::id user_id;               // authenticated user or bridge pup
	string_view bridge_id;             // authenticated bridge

	char id_buf[384];

	request(const method &, const client &, ircd::resource::request &r);
	request() = default;
};

template<class tuple>
struct ircd::m::resource::request::object
:ircd::resource::request::object<tuple>
{
	const m::resource::request &request;

	const decltype(request.access_token) &access_token
	{
		request.access_token
	};

	const decltype(request.node_id) &node_id
	{
		request.node_id
	};

	const decltype(request.user_id) &user_id
	{
		request.user_id
	};

	const decltype(request.bridge_id) &bridge_id
	{
		request.bridge_id
	};

	object(m::resource::request &request)
	:ircd::resource::request::object<tuple>{request}
	,request{request}
	{}
};
