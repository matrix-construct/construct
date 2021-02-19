// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::push
{
	static path params(mutable_buffer, const resource::request &);
	static resource::response handle_delete(client &, const resource::request &);
	static resource::response handle_put(client &, const resource::request &);
	static resource::response handle_get(client &, const resource::request &);

	static const size_t PATH_BUFSIZE {256};
	extern resource::method method_get;
	extern resource::method method_put;
	extern resource::method method_delete;
	extern resource resource;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client 0.6.0-13.13.1.6 :Push Rules API"
};

decltype(ircd::m::push::resource)
ircd::m::push::resource
{
	"/_matrix/client/r0/pushrules",
	{
		"(11.12.1.5) Clients can retrieve, add, modify and remove push"
		" rules globally or per-device"
		,resource::DIRECTORY
	}
};

decltype(ircd::m::push::method_get)
ircd::m::push::method_get
{
	resource, "GET", handle_get,
	{
		method_get.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::push::handle_get(client &client,
                          const resource::request &request)
{
	char buf[PATH_BUFSIZE];
	const auto &path
	{
		params(buf, request)
	};

	const auto &[scope, kind, ruleid]
	{
		path
	};

	const user::pushrules pushrules
	{
		request.user_id
	};

	const bool handle_enabled
	{
		request.parv.size() > 3 &&
		request.parv[3] == "enabled"
	};

	const bool handle_actions
	{
		!handle_enabled &&
		request.parv.size() > 3 &&
		request.parv[3] == "actions"
	};

	if(handle_enabled || handle_actions)
	{
		pushrules.get(path, [&]
		(const auto &event_idx, const auto &path, const json::object &rule)
		{
			const json::member member
			{
				handle_enabled?
					json::member
					{
						"enabled", rule.get("enabled", false)
					}:

				handle_actions?
					json::member
					{
						"actions", rule["actions"]
					}:

				json::member{}
			};

			resource::response
			{
				client, json::members
				{
					member
				}
			};
		});

		return {}; // returned from closure or threw 404
	}

	m::resource::response::chunked response
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

	const auto append_rule{[]
	(json::stack::array &array, const auto &path, const json::object &rule)
	{
		array.append(rule);
	}};

	if(ruleid)
	{
		json::stack::object _scope
		{
			top, scope
		};

		json::stack::array _kind
		{
			_scope, kind
		};

		pushrules.get(std::nothrow, path, [&]
		(const auto &event_idx, const auto &path, const json::object &rule)
		{
			append_rule(_kind, path, rule);
		});

		return {};
	}

	if(kind)
	{
		json::stack::object _scope
		{
			top, scope
		};

		json::stack::array _kind
		{
			_scope, kind
		};

		pushrules.for_each(push::path{scope, kind, {}}, [&]
		(const auto &event_idx, const auto &path, const json::object &rule)
		{
			append_rule(_kind, path, rule);
			return true;
		});

		return {};
	}

	const auto each_scope{[&]
	(const auto &scope)
	{
		json::stack::object _scope
		{
			top, scope
		};

		const auto each_kind{[&]
		(const string_view &kind)
		{
			json::stack::array _kind
			{
				_scope, kind
			};

			pushrules.for_each(push::path{scope, kind, {}}, [&]
			(const auto &event_idx, const auto &path, const json::object &rule)
			{
				append_rule(_kind, path, rule);
				return true;
			});
		}};

		each_kind("content");
		each_kind("override");
		each_kind("room");
		each_kind("sender");
		each_kind("underride");
	}};

	if(scope)
	{
		each_scope(scope);
		return {};
	}

	//TODO: XXX device scopes
	each_scope("global");
	return {};
}

decltype(ircd::m::push::method_put)
ircd::m::push::method_put
{
	resource, "PUT", handle_put,
	{
		method_put.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::push::handle_put(client &client,
                          const resource::request &request)
{
	char buf[PATH_BUFSIZE];
	const auto &path
	{
		params(buf, request)
	};

	const auto &[scope, kind, ruleid]
	{
		path
	};

	if(!scope || !kind || !ruleid)
		throw m::NEED_MORE_PARAMS
		{
			"Missing some path parameters; {scope}/{kind}/{ruleid} required."
		};

	const auto &before
	{
		request.query["before"]
	};

	const auto &after
	{
		request.query["after"]
	};

	const user::pushrules pushrules
	{
		request.user_id
	};

	const json::object &rule
	{
		request
	};

	const bool handle_enabled
	{
		request.parv.size() > 3 &&
		request.parv[3] == "enabled"
	};

	const bool handle_actions
	{
		!handle_enabled &&
		request.parv.size() > 3 &&
		request.parv[3] == "actions"
	};

	if(handle_enabled || handle_actions)
	{
		pushrules.get(path, [&]
		(const auto &event_idx, const auto &path, const json::object &old_rule)
		{
			const auto new_rule
			{
				handle_enabled?
					json::replace(old_rule,
					{
						"enabled", json::get<"enabled"_>(rule)
					}):

				handle_actions?
					json::replace(old_rule,
					{
						"actions", json::get<"actions"_>(rule)
					}):

				json::strung{}
			};

			const auto res
			{
				pushrules.set(path, new_rule)
			};
		});

		return resource::response
		{
			client, http::OK
		};
	}

	const auto new_rule
	{
		json::replace(rule, json::members
		{
			{ "enabled",  rule.get("enabled", true)  },
			{ "default",  false                      },
			{ "rule_id",  ruleid                     },
		})
	};

	const auto res
	{
		pushrules.set(path, new_rule)
	};

	return resource::response
	{
		client, http::OK
	};
}

decltype(ircd::m::push::method_delete)
ircd::m::push::method_delete
{
	resource, "DELETE", handle_delete,
	{
		method_delete.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::push::handle_delete(client &client,
                             const resource::request &request)
{
	char buf[PATH_BUFSIZE];
	const auto &path
	{
		params(buf, request)
	};

	const auto &[scope, kind, ruleid]
	{
		path
	};

	if(!scope || !kind || !ruleid)
		throw m::NEED_MORE_PARAMS
		{
			"Missing some path parameters; {scope}/{kind}/{ruleid} required."
		};

	const user::pushrules pushrules
	{
		request.user_id
	};

	const auto res
	{
		pushrules.del(path)
	};

	return resource::response
	{
		client, http::OK
	};
}

ircd::m::push::path
ircd::m::push::params(mutable_buffer buf,
                      const resource::request &request)
{
	const auto &scope
	{
		request.parv.size() > 0?
			url::decode(buf, request.parv[0]):
			string_view{},
	};

	consume(buf, size(scope));
	const auto &kind
	{
		request.parv.size() > 1?
			url::decode(buf, request.parv[1]):
			string_view{},
	};

	consume(buf, size(kind));
	const auto &ruleid
	{
		request.parv.size() > 2?
			url::decode(buf, request.parv[2]):
			string_view{},
	};

	return path
	{
		scope, kind, ruleid
	};
}
