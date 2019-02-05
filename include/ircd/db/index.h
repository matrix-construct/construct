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
#define HAVE_IRCD_DB_INDEX_H

namespace ircd::db
{
	struct index;
}

/// An index is a glorified column; the database descriptor for this column
/// must specify a prefix-extractor otherwise this index just behaves like
/// a regular key/value column. Index is used to create iterable domains of
/// a column which all share the same key-prefix.
///
/// The index allows a concatenation of two strings to form a key. This con-
/// catenated key is still unique for the column as a whole and is stored as
/// the full concatenation -- however, as stated above the prefix function must
/// be aware of how such a concatenation can be separated back into two
/// strings.
///
/// db::index allows the user to query for either just the first string, or
/// the whole concatenation. In either case, the resulting iterator can move
/// only around the keys and values within the domain of that first string.
/// The iterator presents the user with it.first = second string only, thereby
/// hiding the prefix allowing for easier iteration of the domain.
///
/// Index is not good at reverse iteration due to limitations in RocksDB. Thus
/// it's better to just flip the comparator function for the column.
///
struct ircd::db::index
:column
{
	struct const_iterator_base;
	struct const_iterator;
	struct const_reverse_iterator;

	using iterator_type = const_iterator;
	using const_iterator_type = const_iterator;

	static const gopts applied_opts;

	const_iterator end(const string_view &key, gopts = {});
	const_iterator begin(const string_view &key, gopts = {});
	const_reverse_iterator rend(const string_view &key, gopts = {});
	const_reverse_iterator rbegin(const string_view &key, gopts = {});

	using column::column;
};

struct ircd::db::index::const_iterator_base
:ircd::db::column::const_iterator_base
{
	friend class index;

	const value_type &operator*() const;
	const value_type *operator->() const;

	using column::const_iterator_base::const_iterator_base;

	friend bool seek(index::const_iterator_base &, const string_view &);
	friend bool seek(index::const_iterator_base &, const pos &);
};

struct ircd::db::index::const_iterator
:ircd::db::index::const_iterator_base
{
	friend class index;

	const_iterator &operator++();
	const_iterator &operator--();

	using index::const_iterator_base::const_iterator_base;
};

struct ircd::db::index::const_reverse_iterator
:ircd::db::index::const_iterator_base
{
	friend class index;

	const_reverse_iterator &operator++();
	const_reverse_iterator &operator--();

	using index::const_iterator_base::const_iterator_base;
};
