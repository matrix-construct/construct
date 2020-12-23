// The Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_DB_COLUMN_ITERATOR_H

namespace ircd::db
{
	bool operator==(const column::const_iterator_base &, const column::const_iterator_base &) noexcept;
	bool operator!=(const column::const_iterator_base &, const column::const_iterator_base &) noexcept;
	bool operator<(const column::const_iterator_base &, const column::const_iterator_base &) noexcept;
	bool operator>(const column::const_iterator_base &, const column::const_iterator_base &) noexcept;
}

/// Iteration over all keys down a column. Default construction is an invalid
/// iterator, which could be compared against in the style of STL algorithms.
/// Otherwise, construct an iterator by having it returned from the appropriate
/// function in column::.
///
struct ircd::db::column::const_iterator_base
{
	using key_type = string_view;
	using mapped_type = string_view;
	using value_type = std::pair<key_type, mapped_type>;
	using pointer = value_type *;
	using reference = value_type &;
	using iterator_category = std::bidirectional_iterator_tag;
	using difference_type = size_t;

  protected:
	database::column *c;
	gopts opts;
	std::unique_ptr<rocksdb::Iterator> it;
	mutable value_type val;

	const_iterator_base(database::column *const &, std::unique_ptr<rocksdb::Iterator> &&, db::gopts = {}) noexcept;

  public:
	explicit operator const database::column &() const;
	explicit operator const database::snapshot &() const;
	explicit operator const gopts &() const;

	explicit operator database::column &();
	explicit operator database::snapshot &();
	explicit operator gopts &();

	operator bool() const noexcept;
	bool operator!() const noexcept;

	const value_type *operator->() const;
	const value_type &operator*() const;

	const_iterator_base() noexcept;
	const_iterator_base(const_iterator_base &&) noexcept;
	const_iterator_base &operator=(const_iterator_base &&) noexcept;
	~const_iterator_base() noexcept;

	friend bool operator==(const const_iterator_base &, const const_iterator_base &) noexcept;
	friend bool operator!=(const const_iterator_base &, const const_iterator_base &) noexcept;
	friend bool operator<(const const_iterator_base &, const const_iterator_base &) noexcept;
	friend bool operator>(const const_iterator_base &, const const_iterator_base &) noexcept;

	template<class pos> friend bool seek(column::const_iterator_base &, const pos &);
};

struct ircd::db::column::const_iterator
:const_iterator_base
{
	friend class column;

	const_iterator &operator++();
	const_iterator &operator--();

	using const_iterator_base::const_iterator_base;
};

struct ircd::db::column::const_reverse_iterator
:const_iterator_base
{
	friend class column;

	const_reverse_iterator &operator++();
	const_reverse_iterator &operator--();

	using const_iterator_base::const_iterator_base;
};

inline const ircd::db::column::const_iterator_base::value_type *
ircd::db::column::const_iterator_base::operator->()
const
{
	return &operator*();
}

inline bool
ircd::db::column::const_iterator_base::operator!()
const noexcept
{
	return !static_cast<bool>(*this);
}

inline ircd::db::column::const_iterator_base::operator
gopts &()
{
	return opts;
}

inline ircd::db::column::const_iterator_base::operator
database::snapshot &()
{
	return opts.snapshot;
}

inline ircd::db::column::const_iterator_base::operator
database::column &()
{
	return *c;
}

inline ircd::db::column::const_iterator_base::operator
const gopts &()
const
{
	return opts;
}

inline ircd::db::column::const_iterator_base::operator
const database::snapshot &()
const
{
	return opts.snapshot;
}

inline ircd::db::column::const_iterator_base::operator
const database::column &()
const
{
	return *c;
}
