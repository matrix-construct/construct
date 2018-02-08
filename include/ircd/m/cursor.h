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
#define HAVE_IRCD_M_DBS_CURSOR_H

struct ircd::m::dbs::cursor
{
	template<class index_iterator> struct const_iterator_base;
	struct const_reverse_iterator;
	struct const_iterator;

	using iterator_type = const_iterator;

	db::index index;

	const_iterator end(const string_view &key = {});
	const_iterator begin(const string_view &key = {});

	const_reverse_iterator rend(const string_view &key = {});
	const_reverse_iterator rbegin(const string_view &key = {});

	cursor(const string_view &index)
	:index{*dbs::events, index}
	{}
};

template<class index_iterator>
struct ircd::m::dbs::cursor::const_iterator_base
{
	using value_type = const event;
	using pointer = value_type *;
	using reference = value_type &;
	using difference_type = size_t;
	using iterator_category = std::bidirectional_iterator_tag;

	index_iterator idx;
	std::array<db::cell, event_columns> cell;
	db::row row;
	mutable event v;
	mutable bool stale{true};
	bool invalid{true};

	operator bool() const;
	bool operator!() const;

	bool operator==(const const_iterator_base &o) const;
	bool operator!=(const const_iterator_base &o) const;

	bool row_valid() const;
	string_view row_key() const;
	value_type &operator*() const;
	value_type *operator->() const;

  protected:
	bool seek_row();

  public:
	const_iterator_base &operator++();
	const_iterator_base &operator--();

	const_iterator_base();
	const_iterator_base(const cursor &, index_iterator, const db::gopts & = {});
};

struct ircd::m::dbs::cursor::const_iterator
:const_iterator_base<db::index::const_iterator>
{
	using const_iterator_base<db::index::const_iterator>::const_iterator_base;
};

struct ircd::m::dbs::cursor::const_reverse_iterator
:const_iterator_base<db::index::const_reverse_iterator>
{
	using const_iterator_base<db::index::const_reverse_iterator>::const_iterator_base;
};

//
// cursor
//

inline ircd::m::dbs::cursor::const_reverse_iterator
ircd::m::dbs::cursor::rbegin(const string_view &key)
{
	return const_reverse_iterator { *this, index.rbegin(key), {} };
}

inline ircd::m::dbs::cursor::const_reverse_iterator
ircd::m::dbs::cursor::rend(const string_view &key)
{
	return const_reverse_iterator { *this, index.rend(key), {} };
}

inline ircd::m::dbs::cursor::const_iterator
ircd::m::dbs::cursor::begin(const string_view &key)
{
	return const_iterator { *this, index.begin(key), {} };
}

inline ircd::m::dbs::cursor::const_iterator
ircd::m::dbs::cursor::end(const string_view &key)
{
	return const_iterator { *this, index.end(key), {} };
}

//
// const iterator
//

template<class index_iterator>
ircd::m::dbs::cursor::const_iterator_base<index_iterator>::const_iterator_base(const cursor &c,
                                                                              index_iterator idx,
                                                                              const db::gopts &opts)
:idx{std::move(idx)}
,cell{}
,row
{
	*dbs::events,
	bool(this->idx) && this->idx->second? this->idx->second:
	bool(this->idx)?                      this->idx->first:
	                                      string_view{},
	event{},
	cell,
	opts
}
,stale
{
	true
}
,invalid
{
	!this->idx || !row_valid()
}
{}

template<class index_iterator>
ircd::m::dbs::cursor::const_iterator_base<index_iterator> &
ircd::m::dbs::cursor::const_iterator_base<index_iterator>::operator++()
{
	while(!(invalid = !bool(++idx)))
		if(seek_row())
			break;

	return *this;
}

template<class index_iterator>
ircd::m::dbs::cursor::const_iterator_base<index_iterator> &
ircd::m::dbs::cursor::const_iterator_base<index_iterator>::operator--()
{
	while(!(invalid = !bool(--idx)))
		if(seek_row())
			break;

	return *this;
}

template<class index_iterator>
bool
ircd::m::dbs::cursor::const_iterator_base<index_iterator>::seek_row()
{
	if(!db::seek(row, row_key()))
		return false;

	stale = true;
	return true;
}

template<class index_iterator>
typename ircd::m::dbs::cursor::const_iterator_base<index_iterator>::value_type *
ircd::m::dbs::cursor::const_iterator_base<index_iterator>::operator->()
const
{
	return &this->operator*();
}

template<class index_iterator>
typename ircd::m::dbs::cursor::const_iterator_base<index_iterator>::value_type &
ircd::m::dbs::cursor::const_iterator_base<index_iterator>::operator*()
const
{
	if(!stale)
		return v;

	assign(v, row, row_key());
	stale = false;
	return v;
}

template<class index_iterator>
bool
ircd::m::dbs::cursor::const_iterator_base<index_iterator>::row_valid()
const
{
	return row.valid(row_key());
}

template<class index_iterator>
ircd::string_view
ircd::m::dbs::cursor::const_iterator_base<index_iterator>::row_key()
const
{
	if(!idx)
		return {};

	if(idx->second)
		return idx->second;

	assert(bool(idx->first));
	return idx->first;
}

template<class index_iterator>
bool
ircd::m::dbs::cursor::const_iterator_base<index_iterator>::operator!()
const
{
	return !static_cast<bool>(*this);
}

template<class index_iterator>
ircd::m::dbs::cursor::const_iterator_base<index_iterator>::operator
bool()
const
{
	if(invalid)
		return false;

	if(!idx)
		return false;

	return row_valid();
}

template<class index_iterator>
bool
ircd::m::dbs::cursor::const_iterator_base<index_iterator>::operator!=(const const_iterator_base<index_iterator> &o)
const
{
	return !(*this == o);
}

template<class index_iterator>
bool
ircd::m::dbs::cursor::const_iterator_base<index_iterator>::operator==(const const_iterator_base<index_iterator> &o)
const
{
	if(row_key() != o.row_key())
		return false;

	if(!row_valid() && !o.row_valid())
		return true;

	if(!row_valid() || !o.row_valid())
		return false;

	return true;
}
