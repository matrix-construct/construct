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
#define HAVE_IRCD_DB_ROW_H

namespace ircd::db
{
	struct row;

	// [SET] Delete row from DB (convenience to an op::DELETE delta)
	void del(row &, const sopts & = {});
}

/// A `row` is a collection of cells from different columns which all share the same
/// key. This is an interface for dealing with those cells in the aggregate.
///
/// Note that in a `row` each `cell` comes from a different `column`, but `cell::key()`
/// will all return the same index value across the whole `row`. To get the names
/// of the columns themselves to build ex. the key name of a JSON key-value pair,
/// use `cell::col()`, which will be different for each `cell` across the `row`.
///
/// The db::row::iterator iterates over the cells in a row; to iterate over
/// multiple rows use the db::cursor
struct ircd::db::row
:vector_view<cell>
{
	struct delta;

  public:
	bool valid() const;                          // true on any cell valid; false on empty
	bool valid(const string_view &) const;       // true on any cell valid equal; false on empty

	// [GET] Find cell by name
	const_iterator find(const string_view &column) const;
	iterator find(const string_view &column);

	// [GET] Get cell (or throw)
	const cell &operator[](const size_t &column) const;
	const cell &operator[](const string_view &column) const;
	cell &operator[](const size_t &column);
	cell &operator[](const string_view &column);

    // [SET] Perform operation
	void operator()(const op &, const string_view &col, const string_view &val = {}, const sopts & = {});

	using vector_view<cell>::vector_view;

	row(database &,
	    const string_view &key = {},
	    const vector_view<const string_view> &columns = {},
	    const vector_view<cell> &buf = {},
	    gopts opts = {})
	__attribute__((stack_protect));

	friend size_t seek(row &, const string_view &);
};

namespace ircd::db
{
	// [SET] Perform operations in a sequence as a single transaction. No template
	// iterators supported yet, just a ptr range good for contiguous sequences.
	void write(const row::delta *const &begin, const row::delta *const &end, const sopts & = {});
	void write(const std::initializer_list<row::delta> &, const sopts & = {});
	void write(const sopts &, const std::initializer_list<row::delta> &);
	void write(const row::delta &, const sopts & = {});
}

//
// A delta is an element of a database transaction. You can use this to make
// an all-succeed-or-all-fail commitment to multiple rows at once. It is also
// useful to make a commitment on a single row as a convenient way to compose
// all of a row's cells together.
//
struct ircd::db::row::delta
:std::tuple<op, row *>
{
	enum
	{
		OP, ROW
	};

	delta(row &r, const enum op &op = op::SET)
	:std::tuple<enum op, row *>{op, &r}
	{}

	delta(const enum op &op, row &r)
	:std::tuple<enum op, row *>{op, &r}
	{}
};

inline ircd::db::cell &
ircd::db::row::operator[](const size_t &i)
{
	return vector_view<cell>::at(i);
}

inline const ircd::db::cell &
ircd::db::row::operator[](const size_t &i)
const
{
	return vector_view<cell>::at(i);
}
