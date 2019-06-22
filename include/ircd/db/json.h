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
#define HAVE_IRCD_DB_JSON_H

namespace ircd::db
{
	template<class tuple> bool assign(tuple &, const cell &);
	template<class tuple> bool assign(tuple &, const cell &, const string_view &keyeq);

	template<class tuple> size_t assign(tuple &, const row &);
	template<class tuple> size_t assign(tuple &, const row &, const string_view &keyeq);

	template<class tuple, class... args> tuple make_tuple(args&&...);
}

namespace ircd::db
{
	template<class tuple> void _assign_invalid(tuple &, const cell &);
	template<class tuple> void _assign_valid(tuple &, const cell &);
}

template<class tuple,
         class... args>
tuple
ircd::db::make_tuple(args&&... a)
{
	tuple ret;
	assign(ret, std::forward<args>(a)...);
	return ret;
}

template<class tuple>
size_t
ircd::db::assign(tuple &t,
                 const row &row,
                 const string_view &keyeq)
{
	return std::count_if(std::begin(row), std::end(row), [&t, &keyeq]
	(const auto &cell)
	{
		return assign(t, cell, keyeq);
	});
}

template<class tuple>
size_t
ircd::db::assign(tuple &t,
                 const row &row)
{
	return std::count_if(std::begin(row), std::end(row), [&t]
	(const auto &cell)
	{
		return assign(t, cell);
	});
}

template<class tuple>
bool
ircd::db::assign(tuple &t,
                 const cell &cell)
{
	const bool valid
	{
		cell.valid()
	};

	if(valid)
		_assign_valid(t, cell);
	else
		_assign_invalid(t, cell);

	return valid;
}

template<class tuple>
bool
ircd::db::assign(tuple &t,
                 const cell &cell,
                 const string_view &keyeq)
{
	const bool valid
	{
		cell.valid(keyeq)
	};

	if(valid)
		_assign_valid(t, cell);
	else
		_assign_invalid(t, cell);

	return valid;
}

template<class tuple>
void
ircd::db::_assign_invalid(tuple &t,
                          const cell &cell)
{
	const column &c(cell);
	if(!c)
		return;

	const auto &descriptor
	{
		describe(c)
	};

	const bool is_string
	{
		descriptor.type.second == typeid(string_view)
	};

	if(is_string)
	{
		json::set(t, cell.col(), string_view{});
		return;
	}

	const bool is_integer
	{
		descriptor.type.second == typeid(int64_t) ||
		descriptor.type.second == typeid(double)
	};

	if(is_integer)
	{
		json::set(t, cell.col(), json::undefined_number);
		return;
	}
}

template<class tuple>
void
ircd::db::_assign_valid(tuple &t,
                        const cell &cell)
{
	const column &c(cell);
	const auto &descriptor
	{
		describe(c)
	};

	const bool is_string
	{
		descriptor.type.second == typeid(string_view)
	};

	if(is_string)
		json::set(t, cell.col(), cell.val());
	else
		json::set(t, cell.col(), byte_view<string_view>{cell.val()});
}
