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
	json::property<name::edus, json::array>,
	json::property<name::origin, json::string>,
	json::property<name::origin_server_ts, time_t>,
	json::property<name::pdu_failures, json::array>,
	json::property<name::pdus, json::array>
>
{
	using array = vector_view<const json::value>;
	using closure = std::function<void (json::iov &)>;

	static string_view create_id(const mutable_buffer &out, const string_view &txn);

	static void create(const closure &, const array &pdu, const array &edu = {}, const array &pdu_failure = {});
	static string_view create(const mutable_buffer &, const array &pdu, const array &edu = {}, const array &pdu_failure = {});
	static std::string create(const array &pdu, const array &edu = {}, const array &pdu_failure = {});
	static size_t serialized(const array &pdu, const array &edu = {}, const array &pdu_failure = {});

	using super_type::tuple;
	using super_type::operator=;
};

#pragma GCC diagnostic pop
