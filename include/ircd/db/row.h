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

namespace ircd {
namespace db   {

// A row is a collection of cells from different columns which all share the same
// key. This is an interface for dealing with those cells in the aggregate.
//
struct row
{
	struct delta;
	struct iterator;
	struct const_iterator;
	using value_type = cell &;
	using reference = cell &;
	using pointer = cell *;
	using difference_type = size_t;

  private:
	std::vector<cell> its;

	template<class pos> friend void seek(row &, const pos &);
	friend void seek(row &, const string_view &key);

  public:
	// [GET] Iterations
	const_iterator begin() const;
	const_iterator end() const;
	iterator begin();
	iterator end();

	// [GET] Get iterator to cell
	const_iterator find(const string_view &column) const;
	iterator find(const string_view &column);

	auto empty() const                           { return its.empty();                             }
	auto size() const                            { return its.size();                              }

	// [GET] Get cell
	const cell &operator[](const string_view &column) const;
	cell &operator[](const string_view &column);

    // [SET] Perform operation
	void operator()(const op &, const string_view &col, const string_view &val = {}, const sopts & = {});

	row(std::vector<cell> cells = {})
	:its{std::move(cells)}
	{}

	row(database &,
	    const string_view &key = {},
	    const vector_view<string_view> &columns = {},
	    const gopts &opts = {});

	friend size_t trim(row &, const std::function<bool (cell &)> &);
	friend size_t trim(row &, const string_view &key); // remove invalid or not equal
	friend size_t trim(row &); // remove invalid
};

struct row::const_iterator
{
	using value_type = const cell &;
	using reference = const cell &;
	using pointer = const cell *;
	using iterator_category = std::bidirectional_iterator_tag;

  private:
	friend class row;

	decltype(row::its)::const_iterator it;

	const_iterator(decltype(row::its)::const_iterator it)
	:it{std::move(it)}
	{}

  public:
	reference operator*() const                  { return it.operator*();                          }
	pointer operator->() const                   { return it.operator->();                         }

	const_iterator &operator++()                 { ++it; return *this;                             }
	const_iterator &operator--()                 { --it; return *this;                             }

	const_iterator() = default;

	friend bool operator==(const const_iterator &, const const_iterator &);
	friend bool operator!=(const const_iterator &, const const_iterator &);
};

struct row::iterator
{
	using value_type = cell &;
	using reference = cell &;
	using pointer = cell *;
	using iterator_category = std::bidirectional_iterator_tag;

  private:
	friend class row;

	decltype(row::its)::iterator it;

	iterator(decltype(row::its)::iterator it)
	:it{std::move(it)}
	{}

  public:
	reference operator*() const                  { return it.operator*();                          }
	pointer operator->() const                   { return it.operator->();                         }

	iterator &operator++()                       { ++it; return *this;                             }
	iterator &operator--()                       { --it; return *this;                             }

	iterator() = default;

	friend bool operator==(const iterator &, const iterator &);
	friend bool operator!=(const iterator &, const iterator &);
};

} // namespace db
} // namespace ircd

inline ircd::db::cell &
ircd::db::row::operator[](const string_view &column)
{
	const auto it(find(column));
	if(unlikely(it == end()))
		throw schema_error("column '%s' not specified in the descriptor schema", column);

	return *it;
}

inline const ircd::db::cell &
ircd::db::row::operator[](const string_view &column)
const
{
	const auto it(find(column));
	if(unlikely(it == end()))
		throw schema_error("column '%s' not specified in the descriptor schema", column);

	return *it;
}

inline ircd::db::row::iterator
ircd::db::row::end()
{
	return { std::end(its) };
}

inline ircd::db::row::iterator
ircd::db::row::begin()
{
	return { std::begin(its) };
}

inline ircd::db::row::const_iterator
ircd::db::row::end()
const
{
	return { std::end(its) };
}

inline ircd::db::row::const_iterator
ircd::db::row::begin()
const
{
	return { std::begin(its) };
}

inline bool
ircd::db::operator!=(const row::iterator &a, const row::iterator &b)
{
	return a.it != b.it;
}

inline bool
ircd::db::operator==(const row::iterator &a, const row::iterator &b)
{
	return a.it == b.it;
}

inline bool
ircd::db::operator!=(const row::const_iterator &a, const row::const_iterator &b)
{
	return a.it != b.it;
}

inline bool
ircd::db::operator==(const row::const_iterator &a, const row::const_iterator &b)
{
	return a.it == b.it;
}
