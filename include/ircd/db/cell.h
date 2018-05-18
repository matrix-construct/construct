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
#define HAVE_IRCD_DB_CELL_H

namespace ircd::db
{
	struct cell;

	// Util
	const std::string &name(const cell &);
	uint64_t sequence(const cell &);
}

/// A cell is a single key-value element existing within a column. This structure
/// provides the necessary facilities for working with a single cell. Many simple
/// operations can also be done through the column interface itself so check column.h
/// for satisfaction. Cells from different columns sharing the same key are composed
/// into a `row` (see: row.h).
///
/// When composed into a `row` or `object` remember that calls to cell::key() will
/// all be the same index key -- not the name of the column the cell is representing
/// in the row. You probably want cell::col() when iterating the row to build a
/// JSON object's keys when iterating across a row.
///
/// NOTE that this cell struct is type-agnostic. The database is capable of storing
/// binary data in the key or the value for a cell. The string_view will work with
/// both a normal string and binary data, so this class is not a template and offers
/// no conversions at this level. see: value.h/object.h
///
struct ircd::db::cell
{
	struct delta;

	column c;
	database::snapshot ss;
	std::unique_ptr<rocksdb::Iterator> it;

  public:
	operator const rocksdb::Iterator &() const   { return *it;                                    }
	operator const database::snapshot &() const  { return ss;                                     }
	explicit operator const column &() const     { return c;                                      }
	operator rocksdb::Iterator &()               { return *it;                                    }
	operator database::snapshot &()              { return ss;                                     }
	explicit operator column &()                 { return c;                                      }

	bool valid() const;                          // cell exists
	bool valid(const string_view &) const;       // valid equal key
	bool valid_gt(const string_view &) const;    // valid greater than key
	bool valid_lte(const string_view &) const;   // valid less than or equal key
	operator bool() const                        { return valid();                                }
	bool operator!() const                       { return !valid();                               }

	// [GET] read from cell (zero-copy)
	string_view col() const                      { return name(c);       /* always column name */ }
	string_view key() const;                     // key == index or empty on invalid
	string_view val() const;                     // empty on !valid()
	string_view key();                           // reload then key == index or empty on invalid
	string_view val();                           // reload then empty on !valid()

	// [GET] read from cell (zero-copy)
	explicit operator string_view() const;       // empty on !valid()
	explicit operator string_view();             // reload then empty on !valid()

	// [SET] Perform operation on this cell only
	void operator()(const op &, const string_view &val = {}, const sopts & = {});

	// [SET] assign cell
	cell &operator=(const string_view &);

	// [GET] load cell only (returns valid)
	bool load(const string_view &index = {}, gopts = {});

	cell(column, std::unique_ptr<rocksdb::Iterator>, gopts = {});
	cell(column, const string_view &index, std::unique_ptr<rocksdb::Iterator>, gopts = {});
	cell(column, const string_view &index, gopts = {});
	cell(database &, const string_view &column, const string_view &index, gopts = {});
	cell(database &, const string_view &column, gopts = {});
	cell();
	cell(cell &&) noexcept;
	cell(const cell &) = delete;
	cell &operator=(cell &&) noexcept;
	cell &operator=(const cell &) = delete;
	~cell() noexcept;

	friend std::ostream &operator<<(std::ostream &s, const cell &c);

	template<class pos> friend bool seek(cell &c, const pos &p, gopts = {});
};

namespace ircd::db
{
	// [SET] Perform operations in a sequence as a single transaction. No template
	// iterators supported yet, just a ptr range good for contiguous sequences like
	// vectors and initializer_lists. Alternatively, see: iov.h.
	void write(const cell::delta *const &begin, const cell::delta *const &end, const sopts & = {});
	void write(const std::initializer_list<cell::delta> &, const sopts & = {});
	void write(const sopts &, const std::initializer_list<cell::delta> &);
	void write(const cell::delta &, const sopts & = {});
}

///
/// A delta is an element of a database transaction. You can use this to change
/// the values of cells. Use cell deltas to make an all-succeed-or-all-fail
/// transaction across many cells in various columns at once.
///
struct ircd::db::cell::delta
:std::tuple<op, cell *, string_view>
{
	enum
	{
		OP, CELL, VAL,
	};

	delta(cell &c, const string_view &val, const enum op &op = op::SET)
	:std::tuple<enum op, cell *, string_view>{op, &c, val}
	{}

	delta(const enum op &op, cell &c, const string_view &val = {})
	:std::tuple<enum op, cell *, string_view>{op, &c, val}
	{}
};

inline std::ostream &
ircd::db::operator<<(std::ostream &s, const cell &c)
{
	s << string_view{c};
	return s;
}
