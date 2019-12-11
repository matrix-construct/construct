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
#define HAVE_IRCD_M_RELATES_H

namespace ircd::m
{
	struct relates_to;
}

struct ircd::m::relates_to
:json::tuple
<
	json::property<name::event_id, json::string>,
	json::property<name::rel_type, json::string>
>
{
	using super_type::tuple;
	using super_type::operator=;
};
