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
#define HAVE_IRCD_M_DIRECT_TO_DEVICE_H

namespace ircd::m
{
	struct direct_to_device;
}

struct ircd::m::edu::m_direct_to_device
:json::tuple
<
	json::property<name::sender, json::string>,
	json::property<name::type, json::string>,
	json::property<name::message_id, json::string>,
	json::property<name::messages, json::object>
>
{
	using super_type::tuple;
	using super_type::operator=;
};

struct ircd::m::direct_to_device
:edu::m_direct_to_device
{
	using edu::m_direct_to_device::m_direct_to_device;
};
