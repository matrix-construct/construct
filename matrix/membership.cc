// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::membership_positive)
ircd::m::membership_positive
{
	"join"_sv,
	"invite"_sv,
};

decltype(ircd::m::membership_negative)
ircd::m::membership_negative
{
	"leave"_sv,
	"ban"_sv,
	""_sv,
};

bool
ircd::m::membership(const room &room,
                    const user::id &user_id,
                    const vector_view<const string_view> &membership)
{
	const auto event_idx
	{
		room.get(std::nothrow, "m.room.member", user_id)
	};

	return m::membership(event_idx, membership);
}

bool
ircd::m::membership(const event::idx &event_idx,
                    const vector_view<const string_view> &membership)
{
	bool ret;
	const auto closure
	{
		[&membership, &ret](const json::object &content)
		{
			const json::string &content_membership
			{
				content["membership"]
			};

			const auto it
			{
				std::find(begin(membership), end(membership), content_membership)
			};

			ret = content_membership && it != end(membership);
		}
	};

	// If the query was successful a membership state exists (even if the
	// string found was illegally empty) thus we must return the value of ret.
	if(m::get(std::nothrow, event_idx, "content", closure))
		return ret;

	// If the user included an empty string in the vector, they want us to
	// return true for non-membership (i.e a failed query).
	static const auto &empty{[](auto&& s) { return !s; }};
	if(std::any_of(begin(membership), end(membership), empty))
		return true;

	// If the membership vector itself was empty that's also a non-membership.
	if(membership.empty())
		return true;

	return false;
}

bool
ircd::m::membership(const m::event &event,
                    const vector_view<const string_view> &membership)
{
	const auto it
	{
		std::find(begin(membership), end(membership), m::membership(event))
	};

	return it != end(membership);
}

ircd::string_view
ircd::m::membership(const mutable_buffer &out,
                    const room &room,
                    const user::id &user_id)
{
	const auto event_idx
	{
		room.get(std::nothrow, "m.room.member", user_id)
	};

	return m::membership(out, event_idx);
}

ircd::string_view
ircd::m::membership(const mutable_buffer &out,
                    const event::idx &event_idx)
{
	return m::query(std::nothrow, event_idx, "content", [&out]
	(const json::object &content) -> string_view
	{
		const json::string &content_membership
		{
			content["membership"]
		};

		return strlcpy
		{
			out, content_membership
		};
	});
}

ircd::string_view
ircd::m::membership(const event &event)
{
	const json::object &content
	{
		json::get<"content"_>(event)
	};

	const string_view &membership
	{
		json::get<"membership"_>(event)
	};

	if(membership)
		return membership;

	const json::string &content_membership
	{
		content.get("membership")
	};

	return content_membership;
}
