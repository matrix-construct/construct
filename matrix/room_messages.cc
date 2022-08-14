// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::room::messages::fopts)
ircd::m::room::messages::fopts
{
	event::keys::include
	{
		"content",
	},
};

bool
ircd::m::room::messages::for_each(const closure &closure)
const
{
	event::fetch event
	{
		fopts
	};

	return events.for_each([this, &closure, &event]
	(const string_view &type, const uint64_t &depth, event::idx event_idx)
	{
		assert(type == "m.room.message");
		if(redacted(event_idx))
			return true;

		if(!seek(std::nothrow, event, event_idx))
			return true;

		room::message msg
		{
			json::get<"content"_>(event)
		};

		// Don't show messages which edit other messages, since we will or
		// already have rendered it at the edited message, if we have it,
		// otherwise ignored.
		if(msg.replace_event())
			return true;

		// If the content was replaced msg has to be updated.
		if(m_replace(event, event_idx))
			msg = json::get<"content"_>(event);

		return closure(msg, depth, event_idx);
	});
}

bool
ircd::m::room::messages::m_replace(event &event,
                                   const event::idx &event_idx)
const
{
	// Find the latest edit of this; otherwise stick with the original.
	const m::relates relates
	{
		.refs = event_idx,
		.match_sender = true,
	};

	const event::idx replace_idx
	{
		relates.latest("m.replace")
	};

	if(!replace_idx)
		return false;

	const event::fetch replace
	{
		std::nothrow, replace_idx, fopts
	};

	if(!replace.valid)
		return false;

	const json::object replace_content
	{
		json::get<"content"_>(replace)
	};

	const json::object new_content
	{
		replace_content.get("m.new_content")
	};

	// XXX When m.new_content is undefined the edit does not take place.
	if(!new_content && !replace_content.has("m.new_content"))
		return false;

	json::get<"content"_>(event) = new_content;
	return true;
}
