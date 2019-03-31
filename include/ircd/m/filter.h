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
#define HAVE_IRCD_M_FILTER_H

namespace ircd::m
{
	struct filter;
	struct room_filter;
	struct event_filter;
	struct room_event_filter;
	struct state_filter;

	bool match(const event_filter &, const event &);
	bool match(const room_event_filter &, const event &);
}

/// 5.1 "Filter" we use event_filter here
struct ircd::m::event_filter
:json::tuple
<
	json::property<name::limit, long>,
	json::property<name::types, json::array>,
	json::property<name::senders, json::array>,
	json::property<name::not_types, json::array>,
	json::property<name::not_senders, json::array>
>
{
	using super_type::tuple;
	event_filter(const mutable_buffer &, const json::members &);
	event_filter() = default;
	using super_type::operator=;
};

/// 5.1 "RoomEventFilter"
struct ircd::m::room_event_filter
:json::tuple
<
	json::property<name::limit, long>,
	json::property<name::types, json::array>,
	json::property<name::rooms, json::array>,
	json::property<name::senders, json::array>,
	json::property<name::not_types, json::array>,
	json::property<name::not_rooms, json::array>,
	json::property<name::not_senders, json::array>,
	json::property<name::contains_url, bool>
>
{
	using super_type::tuple;
	room_event_filter(const mutable_buffer &, const json::members &);
	room_event_filter() = default;
	using super_type::operator=;
};

/// "StateFilter"
struct ircd::m::state_filter
:json::tuple
<
	json::property<name::limit, long>,
	json::property<name::types, json::array>,
	json::property<name::rooms, json::array>,
	json::property<name::senders, json::array>,
	json::property<name::not_types, json::array>,
	json::property<name::not_rooms, json::array>,
	json::property<name::not_senders, json::array>,
	json::property<name::contains_url, bool>,
	json::property<name::lazy_load_members, bool>,
	json::property<name::include_redundant_members, bool>
>
{
	using super_type::tuple;
	state_filter(const mutable_buffer &, const json::members &);
	state_filter() = default;
	using super_type::operator=;
};

/// 5.1 "RoomFilter"
struct ircd::m::room_filter
:json::tuple
<
	json::property<name::rooms, json::array>,
	json::property<name::not_rooms, json::array>,
	json::property<name::state, state_filter>,
	json::property<name::timeline, room_event_filter>,
	json::property<name::ephemeral, room_event_filter>,
	json::property<name::account_data, room_event_filter>,
	json::property<name::include_leave, bool>
>
{
	using super_type::tuple;
	room_filter(const mutable_buffer &, const json::members &);
	room_filter() = default;
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
	filter(const user &, const string_view &filter_id, const mutable_buffer &);
	using super_type::operator=;

	static std::string get(const string_view &urle_id_or_json, const m::user &);
};
