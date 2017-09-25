/*
 * charybdis: 21st Century IRC++d
 *
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once
#define HAVE_IRCD_M_FILTER_H

namespace ircd::m
{
	struct filter;
	struct room_filter;
	struct event_filter;
	struct room_event_filter;
}

namespace ircd::m::name
{
	constexpr const char *const event_fields {"event_fields"};
	constexpr const char *const event_format {"event_format"};
	constexpr const char *const account_data {"account_data"};
	constexpr const char *const presence {"presence"};
	constexpr const char *const room {"room"};
	constexpr const char *const timeline {"timeline"};
	constexpr const char *const ephemeral {"ephemeral"};
	constexpr const char *const state {"state"};
	constexpr const char *const rooms {"rooms"};
	constexpr const char *const not_rooms {"not_rooms"};
	constexpr const char *const include_leave {"include_leave"};
	constexpr const char *const types {"types"};
	constexpr const char *const not_types {"not_types"};
	constexpr const char *const senders {"senders"};
	constexpr const char *const not_senders {"not_senders"};
	constexpr const char *const limit {"limit"};
}

struct ircd::m::event_filter
:json::tuple
<
	json::property<name::limit, uint>,
	json::property<name::types, json::array>,
	json::property<name::senders, json::array>,
	json::property<name::not_types, json::array>,
	json::property<name::not_senders, json::array>
>
{
	using super_type::tuple;
	using super_type::operator=;
};

struct ircd::m::room_event_filter
:json::tuple
<
	json::property<name::limit, uint>,
	json::property<name::types, json::array>,
	json::property<name::rooms, json::array>,
	json::property<name::senders, json::array>,
	json::property<name::not_types, json::array>,
	json::property<name::not_rooms, json::array>,
	json::property<name::not_senders, json::array>
>
{
	using super_type::tuple;
	using super_type::operator=;
};

struct ircd::m::room_filter
:json::tuple
<
	json::property<name::rooms, json::array>,
	json::property<name::not_rooms, json::array>,
	json::property<name::state, room_event_filter>,
	json::property<name::timeline, room_event_filter>,
	json::property<name::ephemeral, room_event_filter>,
	json::property<name::account_data, room_event_filter>,
	json::property<name::include_leave, bool>
>
{
	using super_type::tuple;
	using super_type::operator=;
};

struct ircd::m::filter
:json::tuple
<
	json::property<name::event_fields, json::array>,
	json::property<name::event_format, string_view>,
	json::property<name::account_data, event_filter>,
	json::property<name::room, room_filter>,
	json::property<name::presence, event_filter>
>
{
	using super_type::tuple;
	using super_type::operator=;
};
