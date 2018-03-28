// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Federation (undocumented) :Get groups publicised."
};

resource
get_groups_publicised_resource
{
	"/_matrix/federation/v1/get_groups_publicised",
	{
		"Federation (undocumented) publicised groups handler"
	}
};

static resource::response post__groups_publicised(client &, const resource::request &);

resource::method
method_post
{
	get_groups_publicised_resource, "POST", post__groups_publicised,
	{
		method_post.VERIFY_ORIGIN
	}
};

resource::response
post__groups_publicised(client &client,
                        const resource::request &request)
{
	const json::array user_ids
	{
		request["user_ids"]
	};

	static const size_t max{512};
	const size_t count{std::min(size(user_ids), max)};

	std::vector<json::member> users;
	users.reserve(count);

	size_t i(0);
	auto it(begin(user_ids));
	for(; it != end(user_ids) && i < count; ++it, ++i)
	{
		const m::user::id user_id{unquote(*it)};
		users.emplace_back(user_id, json::empty_array);
	}

	return resource::response
	{
		client, json::members
		{
			{ "users", json::value { users.data(), users.size() } }
		}
	};
}
