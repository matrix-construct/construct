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
#define HAVE_IRCD_DB_JSON_H

namespace ircd::db
{
	template<class... T> json::tuple<T...> &set(json::tuple<T...> &, const row &r);
	template<class tuple> tuple make_tuple(const row &r);

	//template<class it, class... T> void deltas(const json::tuple<T...> &tuple, it i);
	template<class it> void set_index(it begin, it end, const string_view &index);
	template<class... T> void write(database &, const string_view &index, const json::tuple<T...> &, const gopts & = {});
}

//
// Commit a json::tuple to the database as a single transaction.
//
template<class it>
void
ircd::db::set_index(it b,
                    it e,
                    const string_view &index)
{
	std::for_each(b, e, [&index]
	(auto &delta)
	{
		std::get<2>(delta) = index;
	});
}

/*
template<class it,
         class... T>
void
ircd::db::deltas(const json::tuple<T...> &tuple,
                 it i)
{
	for_each(tuple, [&i]
	(const string_view &key, const auto &val)
	{
		*i = delta
		{
			key,              // col
			string_view{},    // key  (set_index)
			byte_view<>(val)  // val
		};

		++i;
	});
}
*/

template<class... T>
void
ircd::db::write(database &database,
                const string_view &index,
                const json::tuple<T...> &tuple,
                const gopts &opts)
{
	std::array<delta, tuple.size()> deltas;
	deltas(index, tuple, begin(deltas));
	database(begin(deltas), end(deltas));
}

template<class tuple>
tuple
ircd::db::make_tuple(const row &row)
{
	tuple ret;
	set(ret, row);
	return ret;
}

template<class... T>
ircd::json::tuple<T...> &
ircd::db::set(json::tuple<T...> &tuple,
              const row &row)
{
	for(auto &cell : row)
		if(cell.valid())
			json::set(tuple, cell.col(), cell.val());

	return tuple;
}
