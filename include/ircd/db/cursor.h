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
#define HAVE_IRCD_DB_CURSOR_H

namespace ircd::db
{
	template<database *const &d, class tuple> struct cursor;
}

template<ircd::db::database *const &d,
         class tuple>
struct ircd::db::cursor
{
	template<class index_iterator> struct const_iterator_base;
	struct const_reverse_iterator;
	struct const_iterator;

	using where_type = db::where<tuple>;
	using iterator_type = const_iterator;

	struct index index;
	const where_type *where{nullptr};

	const_iterator end(const string_view &key);
	const_iterator begin(const string_view &key);

	const_reverse_iterator rend(const string_view &key);
	const_reverse_iterator rbegin(const string_view &key);

	cursor(const string_view &index, const where_type *const &where = nullptr)
	:index{*d, index}
	,where{where}
	{}
};

template<ircd::db::database *const &d,
         class tuple>
template<class index_iterator>
struct ircd::db::cursor<d, tuple>::const_iterator_base
{
	using value_type = const tuple;
	using pointer = value_type *;
	using reference = value_type &;
	using difference_type = size_t;
	using iterator_category = std::bidirectional_iterator_tag;
	using where_type = cursor::where_type;

	const where_type *where{nullptr};
	index_iterator idx;
	db::row row;
	mutable tuple v;
	mutable bool stale{true};
	bool invalid{true};

	operator bool() const;
	bool operator!() const;

	bool operator==(const const_iterator_base &o) const;
	bool operator!=(const const_iterator_base &o) const;

	value_type &operator*() const
	{
		if(!stale)
			return v;

		assign(v, row, idx->first);
		stale = false;
		return v;
	}

	value_type *operator->() const
	{
		return &this->operator*();
	}

  protected:
	bool seek_row();

  public:
	const_iterator_base &operator++();
	const_iterator_base &operator--();

	const_iterator_base();
	const_iterator_base(const cursor &, index_iterator, const gopts & = {});
};

template<ircd::db::database *const &d,
         class tuple>
struct ircd::db::cursor<d, tuple>::const_iterator
:const_iterator_base<index::const_iterator>
{
	using const_iterator_base<index::const_iterator>::const_iterator_base;
};

template<ircd::db::database *const &d,
         class tuple>
struct ircd::db::cursor<d, tuple>::const_reverse_iterator
:const_iterator_base<index::const_reverse_iterator>
{
	using const_iterator_base<index::const_reverse_iterator>::const_iterator_base;
};

//
// cursor
//

template<ircd::db::database *const &d,
         class tuple>
typename ircd::db::cursor<d, tuple>::const_reverse_iterator
ircd::db::cursor<d, tuple>::rbegin(const string_view &key)
{
	return const_reverse_iterator { *this, index.rbegin(key), {} };
}

template<ircd::db::database *const &d,
         class tuple>
typename ircd::db::cursor<d, tuple>::const_reverse_iterator
ircd::db::cursor<d, tuple>::rend(const string_view &key)
{
	return const_reverse_iterator { *this, index.rend(key), {} };
}

template<ircd::db::database *const &d,
         class tuple>
typename ircd::db::cursor<d, tuple>::const_iterator
ircd::db::cursor<d, tuple>::begin(const string_view &key)
{
	return const_iterator { *this, index.begin(key), {} };
}

template<ircd::db::database *const &d,
         class tuple>
typename ircd::db::cursor<d, tuple>::const_iterator
ircd::db::cursor<d, tuple>::end(const string_view &key)
{
	return const_iterator { *this, index.end(key), {} };
}

//
// const iterator
//

template<ircd::db::database *const &d,
         class tuple>
template<class index_iterator>
ircd::db::cursor<d, tuple>::const_iterator_base<index_iterator>::const_iterator_base(const cursor &c,
                                                                                     index_iterator idx,
                                                                                     const gopts &opts)
:where{c.where}
,idx{std::move(idx)}
,row
{
	*d, bool(this->idx)? this->idx->first : string_view{}, tuple{}, opts
}
,invalid
{
	!bool(this->idx) || !row.valid(this->idx->first)
}
{
	if(invalid)
		return;

	if(!this->where)
		return;

	if(!(*this->where)(this->operator*()))
		this->operator++();
}

template<ircd::db::database *const &d,
         class tuple>
template<class index_iterator>
ircd::db::cursor<d, tuple>::const_iterator_base<index_iterator> &
ircd::db::cursor<d, tuple>::const_iterator_base<index_iterator>::operator++()
{
	while(!(invalid = !bool(++idx)))
		if(seek_row())
			break;

	return *this;
}

template<ircd::db::database *const &d,
         class tuple>
template<class index_iterator>
ircd::db::cursor<d, tuple>::const_iterator_base<index_iterator> &
ircd::db::cursor<d, tuple>::const_iterator_base<index_iterator>::operator--()
{
	while(!(invalid = !bool(--idx)))
		if(seek_row())
			break;

	return *this;
}

template<ircd::db::database *const &d,
         class tuple>
template<class index_iterator>
bool
ircd::db::cursor<d, tuple>::const_iterator_base<index_iterator>::seek_row()
{
	if(!db::seek(row, idx->first))
		return false;

	stale = true;
	if(this->where && !(*this->where)(this->operator*()))
		return false;

	return true;
}

template<ircd::db::database *const &d,
         class tuple>
template<class index_iterator>
bool
ircd::db::cursor<d, tuple>::const_iterator_base<index_iterator>::operator!()
const
{
	return !static_cast<bool>(*this);
}

template<ircd::db::database *const &d,
         class tuple>
template<class index_iterator>
ircd::db::cursor<d, tuple>::const_iterator_base<index_iterator>::operator
bool()
const
{
	if(invalid)
		return false;

	if(!idx)
		return false;

	return row.valid(idx->first);
}

template<ircd::db::database *const &d,
         class tuple>
template<class index_iterator>
bool
ircd::db::cursor<d, tuple>::const_iterator_base<index_iterator>::operator!=(const const_iterator_base<index_iterator> &o)
const
{
	return !(*this == o);
}

template<ircd::db::database *const &d,
         class tuple>
template<class index_iterator>
bool
ircd::db::cursor<d, tuple>::const_iterator_base<index_iterator>::operator==(const const_iterator_base<index_iterator> &o)
const
{
	if(!row.valid() && !o.row.valid())
		return true;

	if(!row.valid() || !o.row.valid())
		return false;

	return idx->first == o.idx->first;
}
