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
{
	struct delta;

	using vector = std::vector<cell>;
	using iterator = vector::iterator;
	using const_iterator = vector::const_iterator;
	using value_type = vector::value_type;

	vector its;

  public:
	auto empty() const                           { return its.empty();                             }
	auto size() const                            { return its.size();                              }
	bool valid() const;                          // true on any cell valid; false on empty
	bool valid(const string_view &) const;       // true on any cell valid equal; false on empty

	// [GET] Iterations
	const_iterator begin() const;
	const_iterator end() const;
	iterator begin();
	iterator end();

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

	row(std::vector<cell> its = {})
	:its{std::move(its)}
	{}

	row(database &,
	    const string_view &key = {},
	    const vector_view<string_view> &columns = {},
	    gopts opts = {});

	template<class... T>
	row(database &,
	    const string_view &key = {},
	    const json::tuple<T...> & = {},
	    gopts opts = {});

	template<class pos> friend size_t seek(row &, const pos &);
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

template<class... T>
ircd::db::row::row(database &d,
                   const string_view &key,
                   const json::tuple<T...> &t,
                   gopts opts)
:row{[&d, &key, &t, &opts]
() -> row
{
	std::array<string_view, t.size()> cols;
	json::_key_transform(t, std::begin(cols), std::end(cols));
	return { d, key, cols, opts };
}()}
{
}

inline ircd::db::cell &
ircd::db::row::operator[](const size_t &i)
{
	return its.at(i);
}

inline const ircd::db::cell &
ircd::db::row::operator[](const size_t &i)
const
{
	return its.at(i);
}

inline ircd::db::row::iterator
ircd::db::row::end()
{
	return std::end(its);
}

inline ircd::db::row::iterator
ircd::db::row::begin()
{
	return std::begin(its);
}

inline ircd::db::row::const_iterator
ircd::db::row::end()
const
{
	return std::end(its);
}

inline ircd::db::row::const_iterator
ircd::db::row::begin()
const
{
	return std::begin(its);
}
