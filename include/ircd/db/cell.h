/*
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
#define HAVE_IRCD_DB_CELL_H

namespace ircd {
namespace db   {

// A cell is a single key-value element existing within a column. This structure
// provides the necessary facilities for working with a single cell. Many simple
// operations can also be done through the column interface itself so check column.h
// for satisfaction. Cells from different columns sharing the same key are composed
// into a `row` (see: row.h).
//
// NOTE that this cell struct is type-agnostic. The database is capable of storing
// binary data in the key or the value for a cell. The string_view will work with
// both a normal string and binary data, so this class is not a template and offers
// no conversions at this level. see: value.h/object.h
//
struct cell
{
	struct delta;

	column c;
	string_view index;
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

	// [SET] assign cell
	cell &operator=(const string_view &);

	// [GET] -> [SET] assign cell (atomic)
	bool compare_exchange(string_view &expected, const string_view &desired);
	string_view exchange(const string_view &desired);

	bool load(gopts = {});

	cell(column, const string_view &index, std::unique_ptr<rocksdb::Iterator>);
	cell(column, const string_view &index, gopts = {});
	cell(database &, const string_view &column, const string_view &index, gopts = {});
	cell();
	cell(cell &&) noexcept;
	cell(const cell &) = delete;
	cell &operator=(cell &&) noexcept;
	cell &operator=(const cell &) = delete;
	~cell() noexcept;

	friend std::ostream &operator<<(std::ostream &s, const cell &c);
};

//
// A delta is an element of a database transaction. The cell::delta
// is specific to a key in a column. Use cell deltas to make an
// all-succeed-or-all-fail transaction across many cells in many columns.
//
struct cell::delta
:std::tuple<op, cell &, string_view>
{
	delta(const enum op &op, cell &c, const string_view &val = {})
	:std::tuple<enum op, cell &, string_view>{op, c, val}
	{}

	delta(cell &c, const string_view &val, const enum op &op = op::SET)
	:std::tuple<enum op, cell &, string_view>{op, c, val}
	{}
};

// [SET] Perform operations in a sequence as a single transaction.
void write(const cell::delta &, const sopts & = {});
void write(const std::initializer_list<cell::delta> &, const sopts & = {});
void write(const sopts &, const std::initializer_list<cell::delta> &);

const std::string &name(const cell &);
uint64_t sequence(const cell &);

} // namespace db
} // namespace ircd

inline
uint64_t
ircd::db::sequence(const cell &c)
{
	const database::snapshot &ss(c);
	return sequence(ss);
}

inline
const std::string &
ircd::db::name(const cell &c)
{
	return name(c.c);
}

inline std::ostream &
ircd::db::operator<<(std::ostream &s, const cell &c)
{
	s << string_view{c};
	return s;
}
