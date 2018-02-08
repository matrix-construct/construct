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
#define HAVE_IRCD_DB_DELTA_H

namespace ircd::db
{
	struct delta;
	enum op :uint8_t;

	// Indicates an op uses both a key and value for its operation. Some only use
	// a key name so an empty value argument in a delta is okay when false.
	bool value_required(const op &);
	string_view reflect(const op &);
}

/// Types of delta operations
enum ircd::db::op
:uint8_t
{
	GET,                     // no-op sentinel, do not use (debug asserts)
	SET,                     // (batch.Put)
	MERGE,                   // (batch.Merge)
	DELETE,                  // (batch.Delete)
	DELETE_RANGE,            // (batch.DeleteRange)
	SINGLE_DELETE,           // (batch.SingleDelete)
};

/// Update a database cell without `cell`, `column` or row `references`.
///
/// The cell is found by name string. This is the least efficient of the deltas
/// for many updates to the same column or cell when a reference to those can
/// be pre-resolved. This delta has to resolve those references every single
/// time it's iterated over; but that's okay for some transactions.
///
struct ircd::db::delta
:std::tuple<op, string_view, string_view, string_view>
{
	enum // for use with std::get<VAL>(delta)
	{
		OP, COL, KEY, VAL,
	};

	delta(const enum op &op,
	      const string_view &col,
	      const string_view &key,
	      const string_view &val = {})
	:std::tuple<enum op, string_view, string_view, string_view>
	{
		op, col, key, val
	}{}

	delta(const string_view &col,
	      const string_view &key,
	      const string_view &val = {},
	      const enum op &op = op::SET)
	:std::tuple<enum op, string_view, string_view, string_view>
	{
		op, col, key, val
	}{}

	delta() = default;
};
