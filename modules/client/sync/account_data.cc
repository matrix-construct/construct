// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::mapi::header
IRCD_MODULE
{
	"Client Sync :Account Data"
};

namespace ircd::m::sync
{
	static bool account_data_polylog(data &);
	extern item account_data;
}

decltype(ircd::m::sync::account_data)
ircd::m::sync::account_data
{
	"account_data",
	account_data_polylog
};

bool
ircd::m::sync::account_data_polylog(data &data)
{
	json::stack::object out{*data.member};
	json::stack::member member{out, "events"};
	json::stack::array array{member};
	const m::room::state state{data.user_room};
	state.for_each("ircd.account_data", [&data, &array]
	(const m::event &event)
	{
		const auto &event_idx(index(event, std::nothrow));
		if(event_idx < data.since || event_idx > data.current)
			return;

		json::stack::object object{array};

		// type
		{
			json::stack::member member
			{
				object, "type", at<"state_key"_>(event)
			};
		}

		// content
		{
			json::stack::member member
			{
				object, "content", at<"content"_>(event)
			};
		}
	});

	return true;
}
