// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::push
{
	static resource::response handle_pushers_set(client &, const resource::request &);
	extern resource::method pushers_set_post;
	extern resource pushers_set_resource;

	static resource::response handle_pushers_get(client &, const resource::request &);
	extern resource::method pushers_get;
	extern resource pushers_resource;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client r0.6.0-13.13.1 :Pushers"
};

//
// pushers/set
//

decltype(ircd::m::push::pushers_set_resource)
ircd::m::push::pushers_set_resource
{
	"/_matrix/client/r0/pushers/set",
	{
		"(r0.6.0-13.13.1.2) Pushers set",
	}
};

decltype(ircd::m::push::pushers_set_post)
ircd::m::push::pushers_set_post
{
	pushers_set_resource, "POST", handle_pushers_set,
	{
		pushers_set_post.REQUIRES_AUTH |
		pushers_set_post.RATE_LIMITED              
	}
};

ircd::m::resource::response
ircd::m::push::handle_pushers_set(client &client,
                                  const resource::request &request)
{
	const json::object &pusher
	{
		request.content
	};

	const json::string &kind
	{
		pusher.at("kind")
	};

	const m::user::pushers user_pushers
	{
		request.user_id
	};

	if(kind == "null")
	{
		const json::string &key
		{
			pusher.at("pushkey")
		};

		const bool res
		{
			user_pushers.del(key)
		};

		return resource::response
		{
			client, http::OK
		};
	}

	const bool res
	{
		user_pushers.set(pusher)
	};

	return resource::response
	{
		client, http::OK
	};
}

//
// pushers
//

decltype(ircd::m::push::pushers_resource)
ircd::m::push::pushers_resource
{
	"/_matrix/client/r0/pushers",
	{
		"(r0.6.0-13.13.1.1) Pushers",
	}
};

decltype(ircd::m::push::pushers_get)
ircd::m::push::pushers_get
{
	pushers_resource, "GET", handle_pushers_get,
	{
		pushers_get.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::push::handle_pushers_get(client &client,
                                  const resource::request &request)
{
	const m::user::pushers user_pushers
	{
		request.user_id
	};

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::object top
	{
		out
	};

	json::stack::array pushers
	{
		top, "pushers"
	};

	user_pushers.for_each([&pushers]
	(const event::idx &, const string_view &pushkey, const json::object &pusher)
	{
		pushers.append(pusher);
		return true;
	});

	return response;
}
