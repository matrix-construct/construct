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
#define HAVE_IRCD_M_INVITE_3PID_H

namespace ircd::m
{
	struct invite_3pid;
}

struct ircd::m::invite_3pid
:json::tuple
<
	/// Required. The hostname+port of the identity server which should be
	/// used for third party identifier lookups.
	json::property<name::id_server, json::string>,

	/// Required. The kind of address being passed in the address field,
	/// for example email.
	json::property<name::medium, json::string>,

	/// Required. The invitee's third party identifier.
	json::property<name::address, json::string>
>
{
	using super_type::tuple;
};
