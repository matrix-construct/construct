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

	using id_closure = std::function<void (const string_view &)>;
	using node_closure = std::function<void (const json::object &)>;
	using key_closure = std::function<void (const json::array &)>;

	constexpr size_t ID_MAX_SZ    { 64               };
	constexpr size_t KEY_MAX_SZ   { 256 + 256 + 16   };
	constexpr size_t NODE_MAX_SZ  { 4_KiB            };

	int keycmp(const json::array &a, const json::array &b);

	json::array make_key(const mutable_buffer &out, const string_view &type, const string_view &state_key);
	void make_key(const string_view &type, const string_view &state_key, const key_closure &);

	json::object make_node(const mutable_buffer &out, const json::array *const &keys, const size_t &kn, const string_view *const &vals, const size_t &vn);
	json::object make_node(const mutable_buffer &out, const node &old, const size_t &pos, const json::array &key, const string_view &val);
	template<class... args> string_view set_node(db::txn &txn, const mutable_buffer &id, args&&...);

	void get_node(db::column &, const string_view &id, const node_closure &);
	void get_node(const string_view &id, const node_closure &);

	string_view get_head(db::column &, const mutable_buffer &out, const id::room &);
	string_view get_head(const mutable_buffer &out, const id::room &);
	void set_head(db::txn &txn, const id::room &, const string_view &head);

	void get_value(db::column &, const string_view &head, const json::array &key, const id_closure &);
	void get_value(const string_view &head, const json::array &key, const id_closure &);
	void get_value(const string_view &head, const string_view &type, const string_view &state_key, const id_closure &);
	void get_value__room(const id::room &, const string_view &type, const string_view &state_key, const id_closure &);

	void insert(db::txn &txn, const id::room &, const json::array &key, const id::event &);
	void insert(db::txn &txn, const id::room &, const string_view &type, const string_view &state_key, const id::event &);

	void append_nodes(db::txn &, const event &);
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
	size_t keys() const;
	size_t vals() const;
	size_t children() const;

	json::array key(const size_t &) const;
	string_view val(const size_t &) const;

	size_t find(const json::array &key) const;

	using super_type::tuple;
	using super_type::operator=;
};
#pragma GCC diagnostic pop
