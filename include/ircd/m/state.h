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
#define HAVE_IRCD_M_STATE_H

namespace ircd::m::state
{
	struct node;

	size_t keys(const node &);
	size_t vals(const node &);
	size_t children(const node &);
	json::array key(const node &, const size_t &);
	string_view val(const node &, const size_t &);
	int keycmp(const json::array &a, const json::array &b);
	json::array make_key(const mutable_buffer &out, const string_view &type, const string_view &state_key);
	size_t find(const node &, const json::array &key);
	size_t find(const node &, const string_view &type, const string_view &state_key);
	json::object make_node(const mutable_buffer &out, const json::array *const &keys, const size_t &kn, const string_view *const &vals, const size_t &vn);
	json::object make_into(const mutable_buffer &out, const node &old, const size_t &pos, const json::array &key, const string_view &val);

	using id_closure = std::function<void (const string_view &)>;
	using node_closure = std::function<void (const json::object &)>;

	void get_node(db::column &, const string_view &id, const node_closure &);
	void get_node(const string_view &id, const node_closure &);
	string_view set_node(db::iov &txn, const mutable_buffer &hash, const json::array *const &keys, const size_t &kn, const string_view *const &vals, const size_t &vn);
	string_view set_into(db::iov &txn, const mutable_buffer &hash, const node &old, const size_t &pos, const json::array &key, const string_view &val);

	void get_head(db::column &, const id::room &, const id_closure &);
	void get_head(const id::room &, const id_closure &);
	string_view get_head(const id::room &, const mutable_buffer &buf);
	void set_head(db::iov &txn, const id::room &, const string_view &head);

	void get_value(const string_view &head, const json::array &key, const id_closure &);
	void get_value(const string_view &head, const string_view &type, const string_view &state_key, const id_closure &);
	void get_value__room(const id::room &, const string_view &type, const string_view &state_key, const id_closure &);

	void insert(db::iov &txn, const id::room &, const json::array &key, const id::event &);
	void insert(db::iov &txn, const id::room &, const string_view &type, const string_view &state_key, const id::event &);
}

namespace ircd::m::state::name
{
	constexpr const char *const k {"k"};
	constexpr const char *const v {"v"};
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
struct ircd::m::state::node
:json::tuple
<
	json::property<name::k, json::array>,
	json::property<name::v, json::array>
>
{
	using super_type::tuple;
	using super_type::operator=;
};
#pragma GCC diagnostic pop
