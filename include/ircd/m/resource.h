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

struct ircd::m::resource
:ircd::resource
{
	struct method;
	struct request;

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

	string_view origin;
	string_view node_id;
	string_view access_token;
	m::user::id::buf user_id;

	request(const method &, const client &, ircd::resource::request &r);
	request() = default;
};

template<class tuple>
struct ircd::m::resource::request::object
:ircd::resource::request::object<tuple>
{
	const m::resource::request &r;
	const decltype(r.origin) &origin;
	const decltype(r.user_id) &user_id;
	const decltype(r.node_id) &node_id;
	const decltype(r.access_token) &access_token;

	object(m::resource::request &r)
	:ircd::resource::request::object<tuple>{r}
	,r{r}
	,origin{r.origin}
	,user_id{r.user_id}
	,node_id{r.node_id}
	,access_token{r.access_token}
	{}
};
