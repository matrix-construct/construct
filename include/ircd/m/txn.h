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
#define HAVE_IRCD_M_TXN_H
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"

namespace ircd::m
{
	struct txn;
}

struct ircd::m::txn
:json::tuple
<
	json::property<name::edus, string_view>,
	json::property<name::origin, string_view>,
	json::property<name::origin_server_ts, time_t>,
	json::property<name::pdu_failures, string_view>,
	json::property<name::pdus, string_view>
>
{
	using super_type::tuple;
	using super_type::operator=;
};

#pragma GCC diagnostic pop
