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
#define HAVE_IRCD_M_TYPING_H

namespace ircd::m
{
	struct typing;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
struct ircd::m::edu::m_typing
:json::tuple
<
	json::property<name::user_id, json::string>,
	json::property<name::room_id, json::string>,
	json::property<name::timeout, time_t>,
	json::property<name::typing, bool>
>
{
	using super_type::tuple;
	using super_type::operator=;
};
#pragma GCC diagnostic pop

struct ircd::m::typing
:m::edu::m_typing
{
	struct commit;

	using edu::m_typing::m_typing;
};

struct ircd::m::typing::commit
:event::id::buf
{
	commit(const typing &);
};
