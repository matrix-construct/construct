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
#define HAVE_IRCD_M_REGISTER_H

namespace ircd::m
{
	struct registar;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
struct ircd::m::registar
:json::tuple
<
	json::property<name::username, string_view>,
	json::property<name::bind_email, bool>,
	json::property<name::password, string_view>,
	json::property<name::auth, json::object>,
	json::property<name::device_id, string_view>
>
{
	using super_type::tuple;
};
#pragma GCC diagnostic pop
