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

	static string_view path_version(const string_view &);
	static string_view path_canonize(const mutable_buffer &, const string_view &);

  private:
	char path_buf[512];

	ircd::resource &route(const string_view &path) const override;
	string_view params(const string_view &path) const override;

  public:
	resource(const string_view &path, struct opts);
	resource(const string_view &path);
	~resource() noexcept override;
};

struct ircd::m::resource::method
:ircd::resource::method
{
	enum flag :std::underlying_type_t<ircd::resource::flag>;
	using handler = std::function<response (client &, request &)>;

	handler function;

	response handle(client &, ircd::resource::request &);

	method(m::resource &, const string_view &name, handler, struct opts = {});
	~method() noexcept;
};

/// Matrix resource method option flags. These flags are valued in the upper
/// bits to not conflict with the base ircd::resource flag values.
enum ircd::m::resource::method::flag
:std::underlying_type_t<ircd::resource::flag>
{
	/// Method will verify access_token or authentication bearer. This is used
	/// on the client-server API.
	REQUIRES_AUTH = 0x0001'0000,

	/// Method will verify X-Matrix-authorization. This is used on the
	/// federation API.
	VERIFY_ORIGIN = 0x0002'0000,

	/// Method requires operator access. This is used on the client-server API.
	REQUIRES_OPER = 0x0004'0000,
};

struct ircd::m::resource::request
:ircd::resource::request
{
	template<class> struct object;

	string_view version;               // api version
	pair<string_view> authorization;   // proffering any
	string_view access_token;          // proffering user
	m::request::x_matrix x_matrix;     // proferring server

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

	const decltype(request.version) &version
	{
		request.version
	};

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
