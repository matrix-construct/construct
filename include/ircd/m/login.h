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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
struct ircd::m::login
:json::tuple
<
	/// Required. The login type being used. One of: ["m.login.password",
	/// "m.login.token"]
	json::property<name::type, string_view>,

	/// The fully qualified user ID or just local part of the user ID, to
	/// log in.
	json::property<name::user, string_view>,

	/// When logging in using a third party identifier, the medium of the
	/// identifier. Must be 'email'.
	json::property<name::medium, string_view>,

	/// Third party identifier for the user.
	json::property<name::address, string_view>,

	/// Required when type is m.login.password. The user's password.
	json::property<name::password, string_view>,

	/// Required when type is m.login.token. The login token.
	json::property<name::token, string_view>,

	/// ID of the client device. If this does not correspond to a known client
	/// device, a new device will be created. The server will auto-generate a
	/// device_id if this is not specified.
	json::property<name::device_id, string_view>,

	/// A display name to assign to the newly-created device. Ignored if
	/// device_id corresponds to a known device.
	json::property<name::initial_device_display_name, string_view>
>
{
	using super_type::tuple;
};
#pragma GCC diagnostic pop
