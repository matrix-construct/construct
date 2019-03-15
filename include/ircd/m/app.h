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
#define HAVE_IRCD_M_APP_H

namespace ircd::m::app
{
	struct namespace_;
	struct namespaces;
	struct config;

	bool exists(const string_view &id);

	extern log::log log;
}

struct ircd::m::app::namespace_
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

struct ircd::m::app::namespaces
:json::tuple
<
	/// Events which are sent from certain users.
	json::property<name::users, namespace_>,

	/// Events which are sent in rooms with certain room aliases.
	json::property<name::aliases, namespace_>,

	/// Events which are sent in rooms with certain room IDs.
	json::property<name::rooms, namespace_>
>
{
	using super_type::tuple;
};

struct ircd::m::app::config
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
	static event::idx idx(std::nothrow_t, const string_view &id);
	static event::idx idx(const string_view &id);

	static bool get(std::nothrow_t, const string_view &id, const event::fetch::view_closure &);
	static void get(const string_view &id, const event::fetch::view_closure &);
	static std::string get(std::nothrow_t, const string_view &id);
	static std::string get(const string_view &id);

	using super_type::tuple;
};
