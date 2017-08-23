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
#define HAVE_IRCD_DB_OBJECT_H

#include "value.h"

namespace ircd {
namespace db   {

template<database *const &d>
struct object
:row
{
	struct const_iterator;

	string_view prefix;
	string_view index;

	template<class T = string_view> value<d, T> get(const string_view &name);

	object(const string_view &index, const string_view &prefix = {}, gopts = {});
	object(const string_view &index, const std::initializer_list<json::index::member> &, gopts = {});
	object() = default;
};

} // namespace db
} // namespace ircd

template<ircd::db::database *const &d>
ircd::db::object<d>::object(const string_view &index,
	                        const string_view &prefix,
	                        gopts opts)
:row{[&prefix, &index, &opts]() -> row
{
	// The prefix is the name of the object we want to find members in.
	// This function has to find columns starting with the prefix but not
	// containing any additional '.' except the one directly after the prefix,
	// as more '.' indicates sub-members which we don't fetch here.

	auto &columns(d->columns);
	auto low(columns.lower_bound(prefix)), hi(low);
	for(; hi != std::end(columns); ++hi)
	{
		const auto &name(hi->first);
		if(!startswith(name, prefix))
			break;
	}

	string_view names[std::distance(low, hi)];
	std::transform(low, hi, names, [&prefix](const auto &pair)
	{
		const auto &path(pair.first);

		// Find members of this object by removing the prefix and then removing
		// any members which have a '.' indicating they are not at this level.
		string_view name(path);
		name = lstrip(name, prefix);
		name = lstrip(name, '.');
		if(tokens_count(name, ".") != 1)
			return string_view{};

		return string_view{path};
	});

	// Clear empty names from the array before passing up to row{}
	const auto end(std::remove(names, names + std::distance(low, hi), string_view{}));
	const auto count(std::distance(names, end));
	return
	{
		*d, index, vector_view<string_view>(names, count), opts
	};
}()}
,index{index}
{
}

template<ircd::db::database *const &d>
ircd::db::object<d>::object(const string_view &index,
                            const std::initializer_list<json::index::member> &members,
                            gopts opts)
:object{index, string_view{}, opts}
{
}

template<ircd::db::database *const &d>
template<class T>
ircd::db::value<d, T>
ircd::db::object<d>::get(const string_view &name)
{
	return row::operator[](name);
}
