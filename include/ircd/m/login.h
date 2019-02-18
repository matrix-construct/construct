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
#define HAVE_IRCD_M_LOGIN_H

namespace ircd::m
{
	struct login;
}

struct ircd::m::login
:json::tuple
<
	/// Required. The login type being used. One of: ["m.login.password",
	/// "m.login.token"]
	json::property<name::type, json::string>,

	/// The fully qualified user ID or just local part of the user ID, to
	/// log in.
	json::property<name::user, json::string>,

	/// When logging in using a third party identifier, the medium of the
	/// identifier. Must be 'email'.
	json::property<name::medium, json::string>,

	/// Third party identifier for the user.
	json::property<name::address, json::string>,

	/// Required when type is m.login.password. The user's password.
	json::property<name::password, json::string>,

	/// Required when type is m.login.token. The login token.
	json::property<name::token, json::string>,

	/// ID of the client device. If this does not correspond to a known client
	/// device, a new device will be created. The server will auto-generate a
	/// device_id if this is not specified.
	json::property<name::device_id, json::string>,

	/// A display name to assign to the newly-created device. Ignored if
	/// device_id corresponds to a known device.
	json::property<name::initial_device_display_name, json::string>
>
{
	using super_type::tuple;
};
