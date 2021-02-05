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
#define HAVE_IRCD_M_BRIDGE_H

namespace ircd::m::bridge
{
	struct namespace_;
	struct namespaces;
	struct config;
	struct query;

	string_view make_uri(const mutable_buffer &, const config &, const string_view &path);

	bool exists(const config &, const m::user::id &);
	bool exists(const config &, const m::room::alias &);

	json::object protocol(const mutable_buffer &, const config &, const string_view &name);

	extern log::log log;
}

struct ircd::m::bridge::query
{
	static conf::item<seconds> timeout;

	rfc3986::uri base_url;
	unique_mutable_buffer buf;
	string_view uri;
	window_buffer wb;
	http::request hypertext;
	server::request::opts sopts;
	server::request request;
	http::code code;

	query(const config &, const string_view &uri, const mutable_buffer &resp_body = {});
};

struct ircd::m::bridge::namespace_
:json::tuple
<
	/// Required. A true or false value stating whether this application
	/// service has exclusive access to events within this namespace.
	json::property<name::exclusive, bool>,

	/// Required. A regular expression defining which values this namespace
	/// includes.
	json::property<name::regex, json::string>
>
{
	using super_type::tuple;
};

struct ircd::m::bridge::namespaces
:json::tuple
<
	/// Events which are sent from certain users.
	json::property<name::users, json::array>,

	/// Events which are sent in rooms with certain room aliases.
	json::property<name::aliases, json::array>,

	/// Events which are sent in rooms with certain room IDs.
	json::property<name::rooms, json::array>
>
{
	using super_type::tuple;
};

struct ircd::m::bridge::config
:json::tuple
<
	/// Required. A unique, user-defined ID of the application service which
	/// will never change.
	json::property<name::id, json::string>,

	/// Required. The URL for the application service. May include a path
	/// after the domain name. Optionally set to null if no traffic is
	/// required.
	json::property<name::url, json::string>,

	/// Required. A unique token for application services to use to
	/// authenticate requests to Homeservers.
	json::property<name::as_token, json::string>,

	/// Required. A unique token for Homeservers to use to authenticate
	/// requests to application services.
	json::property<name::hs_token, json::string>,

	/// Required. The localpart of the user associated with the application
	/// service.
	json::property<name::sender_localpart, json::string>,

	/// Required. A list of users, aliases and rooms namespaces that the application service controls.
	json::property<name::namespaces, namespaces>,

	/// Whether requests from masqueraded users are rate-limited. The sender is excluded.
	json::property<name::rate_limited, bool>,

	/// The external protocols which the application service provides (e.g. IRC).
	json::property<name::protocols, json::array>
>
{
	using closure_bool = std::function<bool (const event::idx &, const event &, const config &)>;
	using closure = std::function<void (const event::idx &, const event &, const config &)>;

	static bool for_each(const closure_bool &);
	static bool get(std::nothrow_t, const string_view &id, const closure &);
	static void get(const string_view &id, const closure &);
	static bool exists(const string_view &id);

	using super_type::tuple;
};
