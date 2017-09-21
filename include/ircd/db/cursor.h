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
	struct const_iterator;

	using where_type = db::where<tuple>;
	using iterator_type = const_iterator;
	using const_iterator_type = const_iterator;
	using index_iterator = ircd::db::index::const_iterator;

	struct index index;
	const where_type *where{nullptr};

	const_iterator end();
	const_iterator begin();
	const_iterator find(const string_view &key);

	cursor(const string_view &index)
	:index{*d, index}
	,where{nullptr}
	{}
};

template<ircd::db::database *const &d,
         class tuple>
struct ircd::db::cursor<d, tuple>::const_iterator
{
	using value_type = const tuple;
	using pointer = value_type *;
	using reference = value_type &;
	using difference_type = size_t;
	using iterator_category = std::bidirectional_iterator_tag;
	using index_iterator = ircd::db::index::const_iterator;
	using where_type = cursor::where_type;

	const where_type *where{nullptr};
	index_iterator idx;
	db::row row;
	mutable tuple v;
	mutable bool stale{true};
	bool invalid{false};

	operator bool() const;
	bool operator!() const;

	bool operator==(const const_iterator &o) const;
	bool operator!=(const const_iterator &o) const;

	value_type &operator*() const;
	value_type *operator->() const;

  private:
	bool seek(const pos &p);

  public:
	const_iterator &operator++();
	const_iterator &operator--();

	const_iterator();
	const_iterator(const cursor &, index_iterator, const gopts & = {});
};

//
// cursor
//

template<ircd::db::database *const &d,
         class tuple>
typename ircd::db::cursor<d, tuple>::const_iterator
ircd::db::cursor<d, tuple>::find(const string_view &key)
{
	return const_iterator { *this, index.find(key), {} };
}

template<ircd::db::database *const &d,
         class tuple>
typename ircd::db::cursor<d, tuple>::const_iterator
ircd::db::cursor<d, tuple>::begin()
{
	return const_iterator { *this, index.begin(), {} };
}

template<ircd::db::database *const &d,
         class tuple>
typename ircd::db::cursor<d, tuple>::const_iterator
ircd::db::cursor<d, tuple>::end()
{
	return {};
}

//
// const iterator
//

template<ircd::db::database *const &d,
         class tuple>
ircd::db::cursor<d, tuple>::const_iterator::const_iterator()
:invalid{true}
{
}

template<ircd::db::database *const &d,
         class tuple>
ircd::db::cursor<d, tuple>::const_iterator::const_iterator(const cursor &c,
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
		seek(pos::NEXT);
}

template<ircd::db::database *const &d,
         class tuple>
typename ircd::db::cursor<d, tuple>::const_iterator &
ircd::db::cursor<d, tuple>::const_iterator::operator++()
{
	seek(pos::NEXT);
	return *this;
}

template<ircd::db::database *const &d,
         class tuple>
typename ircd::db::cursor<d, tuple>::const_iterator &
ircd::db::cursor<d, tuple>::const_iterator::operator--()
{
	seek(pos::PREV);
	return *this;
}

template<ircd::db::database *const &d,
         class tuple>
bool
ircd::db::cursor<d, tuple>::const_iterator::seek(const pos &p)
{
	while(!(invalid = !db::seek(idx, p)))
	{
		if(db::seek(row, idx->first))
		{
			stale = true;
			if(!this->where || (*this->where)(this->operator*()))
				return true;
		}
	}

	return false;
}

template<ircd::db::database *const &d,
         class tuple>
const typename ircd::db::cursor<d, tuple>::const_iterator::value_type *
ircd::db::cursor<d, tuple>::const_iterator::operator->()
const
{
	return &operator*();
}

template<ircd::db::database *const &d,
         class tuple>
const typename ircd::db::cursor<d, tuple>::const_iterator::value_type &
ircd::db::cursor<d, tuple>::const_iterator::operator*()
const
{
	if(!stale)
		return v;

	assign(v, row, idx->first);
	stale = false;
	return v;
}

template<ircd::db::database *const &d,
         class tuple>
bool
ircd::db::cursor<d, tuple>::const_iterator::operator!()
const
{
	return !static_cast<bool>(*this);
}

template<ircd::db::database *const &d,
         class tuple>
ircd::db::cursor<d, tuple>::const_iterator::operator
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
bool
ircd::db::cursor<d, tuple>::const_iterator::operator!=(const const_iterator &o)
const
{
	return !(*this == o);
}

template<ircd::db::database *const &d,
         class tuple>
bool
ircd::db::cursor<d, tuple>::const_iterator::operator==(const const_iterator &o)
const
{
	if(!row.valid() && !o.row.valid())
		return true;

	if(!row.valid() || !o.row.valid())
		return false;

	return idx->first == o.idx->first;
}
