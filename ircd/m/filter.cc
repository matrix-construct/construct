// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/m/m.h>

const ircd::m::room::id::buf
filters_room_id
{
	"filters", ircd::my_host()
};

ircd::m::room
ircd::m::filter::filters
{
	filters_room_id
};

ircd::m::filter::filter(const string_view &filter_id,
                        const mutable_buffer &buf)
{
	size_t len{0};
	filters.get("ircd.filter"_sv, filter_id, [&buf, &len]
	(const m::event &event)
	{
		len = copy(buf, json::get<"content"_>(event));
	});

	new (this) filter{json::object{buf}};
}

size_t
ircd::m::filter::size(const string_view &filter_id)
{
	size_t len{0};
	filters.get("ircd.filter"_sv, filter_id, [&len]
	(const m::event &event)
	{
		const string_view filter
		{
			json::get<"content"_>(event)
		};

		len = size(filter);
	});

	return len;
}
