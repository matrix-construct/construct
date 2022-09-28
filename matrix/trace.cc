// Matrix Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

bool
ircd::m::trace::for_each(const event::idx &event_idx,
                         const closure &closure)
{
	event::fetch event
	{
		std::nothrow, event_idx
	};

	if(!event.valid)
		return true;

	if(redacted(event))
		return true;

	room::messages messages
	{
		room::id{json::get<"room_id"_>(event)},
		{
			json::get<"depth"_>(event), -1L
		}
	};

	bool ret(true), loop; do
	{
		loop = false;
		messages.for_each([&closure, &event, &messages, &loop, &ret]
		(const room::message &msg, const int64_t &depth, const event::idx &event_idx)
		{
			// Ignore messages at the same depth as the initial
			if(event.event_idx != event_idx && depth == json::get<"depth"_>(event))
				return true;

			// Call user; check if they want to break iteration
			if(!(ret = closure(event_idx, depth, msg)))
				return false;

			const auto reply_to_id
			{
				msg.reply_to_event()
			};

			// If this is not a reply continue to the prior message.
			if(!reply_to_id)
				return true;

			// If we don't have the message being replied to, break here
			if(!seek(std::nothrow, event, reply_to_id))
				return false;

			// The message replied to was redacted, break here.
			if(redacted(event))
				return false;

			// Jump to the message being replied to
			messages =
			{
				room::id{json::get<"room_id"_>(event)},
				{
					json::get<"depth"_>(event), -1L
				}
			};

			loop = true;
			return false;
		});
	}
	while(ret && loop);

	return ret;
}
