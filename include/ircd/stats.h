// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_STATS_H

namespace ircd::stats
{
	struct item;
	using value_type = int128_t;

	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, not_found)

	extern std::map<string_view, item *> items;

	const value_type &get(const item &);
	value_type &get(item &);

	value_type &inc(item &, const value_type & = 1);
	value_type &dec(item &, const value_type & = 1);
	value_type &set(item &, const value_type & = 0);

	std::ostream &operator<<(std::ostream &, const item &);
}

struct ircd::stats::item
{
	static const size_t NAME_MAX_LEN;

	json::strung feature_;
	json::object feature;
	string_view name;
	value_type val;

  public:
	explicit operator const value_type &() const;
	explicit operator value_type &();
	bool operator!() const;

	item &operator+=(const value_type &) &;
	item &operator-=(const value_type &) &;
	item &operator=(const value_type &) &;
	item &operator++();
	item &operator--();

	item(const json::members &);
	item(item &&) = delete;
	item(const item &) = delete;
	~item() noexcept;
};

inline ircd::stats::item &
ircd::stats::item::operator--()
{
	--val;
	return *this;
}

inline ircd::stats::item &
ircd::stats::item::operator++()
{
	++val;
	return *this;
}

inline ircd::stats::item &
ircd::stats::item::operator=(const value_type &v)
&
{
	set(*this, v);
	return *this;
}

inline ircd::stats::item &
ircd::stats::item::operator-=(const value_type &v)
&
{
	dec(*this, v);
	return *this;
}

inline ircd::stats::item &
ircd::stats::item::operator+=(const value_type &v)
&
{
	inc(*this, v);
	return *this;
}

inline bool
ircd::stats::item::operator!()
const
{
	return !get(*this);
}

inline ircd::stats::item::operator
value_type &()
{
	return get(*this);
}

inline ircd::stats::item::operator
const value_type &()
const
{
	return get(*this);
}

inline ircd::stats::value_type &
ircd::stats::set(item &item,
                 const value_type &v)
{
	item.val = v;
	return get(item);
}

inline ircd::stats::value_type &
ircd::stats::dec(item &item,
                 const value_type &n)
{
	item.val -= n;
	return get(item);
}

inline ircd::stats::value_type &
ircd::stats::inc(item &item,
                 const value_type &n)
{
	item.val += n;
	return get(item);
}

inline ircd::stats::value_type &
ircd::stats::get(item &item)
{
	return item.val;
}

inline const ircd::stats::value_type &
ircd::stats::get(const item &item)
{
	return item.val;
}
