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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"

namespace ircd::m
{
	struct filter;
	struct room_filter;
	struct event_filter;
	struct room_event_filter;
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
	static room filters;

	using super_type::tuple;
	using super_type::operator=;

	static size_t size(const string_view &filter_id);

	filter(const string_view &filter_id, const mutable_buffer &);
};

#pragma GCC diagnostic pop
