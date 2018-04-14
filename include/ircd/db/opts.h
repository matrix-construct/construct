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
#define HAVE_IRCD_DB_OPTS_H

namespace ircd::db
{
	enum class set :uint64_t;
	enum class get :uint64_t;

	template<class> struct opts;
	struct sopts;
	struct gopts;

	template<class T> bool test(const opts<T> &, const typename std::underlying_type<T>::type &);
	template<class T> bool test(const opts<T> &, const T &value);

	template<class T> opts<T> &operator|=(opts<T> &, const opts<T> &);
	template<class T> opts<T> &operator|=(opts<T> &, const T &value);

	template<class T> opts<T> &operator&=(opts<T> &, const opts<T> &);
	template<class T> opts<T> &operator&=(opts<T> &, const T &value);
}

enum class ircd::db::set
:uint64_t
{
	FSYNC            = 0x0001, // Uses kernel filesystem synchronization after write (slow)
	NO_JOURNAL       = 0x0002, // Write Ahead Log (WAL) for some crash recovery
	MISSING_COLUMNS  = 0x0004, // No exception thrown when writing to a deleted column family
};

enum class ircd::db::get
:uint64_t
{
	PIN              = 0x0001, // Keep iter data in memory for iter lifetime (good for lots of ++/--)
	CACHE            = 0x0002, // Update the cache (CACHE is default for non-iterator operations)
	NO_CACHE         = 0x0004, // Do not update the cache (NO_CACHE is default for iterators)
	NO_SNAPSHOT      = 0x0008, // This iterator will have the latest data (tailing)
	NO_CHECKSUM      = 0x0010, // Integrity of data will be checked unless this is specified
	PREFIX           = 0x0020, // (prefix_same_as_start); automatic for index columns with pfx
	ORDERED          = 0x0040, // (total_order_seek); relevant to index columns
};

template<class T>
struct ircd::db::opts
{
	using flag_t = typename std::underlying_type<T>::type;

	flag_t value {0};

	opts() = default;
	opts(const std::initializer_list<T> &list)
	:value{combine_flags(list)}
	{}
};

struct ircd::db::sopts
:opts<set>
{
	using opts<set>::opts;
};

struct ircd::db::gopts
:opts<get>
{
	database::snapshot snapshot;
	const rocksdb::Slice *lower_bound { nullptr };
	const rocksdb::Slice *upper_bound { nullptr };
	size_t readahead { 0 };
	uint64_t seqnum { 0 };

	using opts<get>::opts;
};

template<class T>
ircd::db::opts<T> &
ircd::db::operator&=(opts<T> &a,
                     const T &value)
{
	using flag_t = typename opts<T>::flag_t;

	a.value &= flag_t(value);
	return a;
}

template<class T>
ircd::db::opts<T> &
ircd::db::operator&=(opts<T> &a,
                     const opts<T> &b)
{
	a.value &= b.value;
	return a;
}

template<class T>
ircd::db::opts<T> &
ircd::db::operator|=(opts<T> &a,
                     const T &value)
{
	using flag_t = typename opts<T>::flag_t;

	a.value |= flag_t(value);
	return a;
}

template<class T>
ircd::db::opts<T> &
ircd::db::operator|=(opts<T> &a,
                     const opts<T> &b)
{
	a.value |= b.value;
	return a;
}

template<class T>
bool
ircd::db::test(const opts<T> &a,
               const T &value)
{
	using flag_t = typename opts<T>::flag_t;

	return a.value & flag_t(value);
}

template<class T>
bool
ircd::db::test(const opts<T> &a,
               const typename std::underlying_type<T>::type &value)
{
	return (a.value & value) == value;
}
