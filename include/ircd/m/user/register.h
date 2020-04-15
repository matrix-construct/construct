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
#define HAVE_IRCD_M_USER_REGISTER_H

/// Register a new user. The principal function is operator() which acts on the
/// data provided in the structure (otherwise the structure itself is inert
/// (see json/tuple.h)). This is a superset of m::create(user::id), which is
/// called internally and only one part of the process. This function returns
/// a spec JSON object [into the buffer] which could be returned from an
/// `r0/register` endpoint. The IP argument is optional, used for information
/// about the registrant if a client; to programmatically register a user,
/// simply fill out the structure as best as possible otherwise.
///
struct ircd::m::user::registar
:json::tuple
<
	json::property<name::username, json::string>,
	json::property<name::type, json::string>,
	json::property<name::bind_email, bool>,
	json::property<name::password, json::string>,
	json::property<name::auth, json::object>,
	json::property<name::device_id, json::string>,
	json::property<name::inhibit_login, bool>,
	json::property<name::initial_device_display_name, json::string>
>
{
	static void validate_user_id(const m::user::id &);
	static void validate_password(const string_view &);

	json::object operator()(const mutable_buffer &out, const net::ipport &remote = {}) const;

	using super_type::tuple;
};
