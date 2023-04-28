// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

namespace
{
	using user_devices_map = std::map<m::user::id, json::array>;
	using host_users_map = std::map<string_view, user_devices_map>;
	using query_map = std::map<string_view, m::fed::user::keys::query>;
	using failure_map = std::map<string_view, std::exception_ptr, std::less<>>;
	using buffer_list = std::vector<unique_buffer<mutable_buffer>>;
}

static void
handle_cross_keys(const m::resource::request &,
                  const user_devices_map &,
                  query_map &,
                  failure_map &,
                  json::stack::object &,
                  const string_view &);

static void
handle_device_keys(const m::resource::request &,
                   const user_devices_map &,
                   query_map &,
                   failure_map &,
                   json::stack::object &);

static void
handle_responses(const m::resource::request &,
                 const host_users_map &,
                 query_map &,
                 failure_map &,
                 json::stack::object &);

static void
handle_errors(const m::resource::request &,
              query_map &,
              failure_map &);

static bool
send_request(const string_view &,
             const user_devices_map &,
             failure_map &,
             buffer_list &,
             query_map &);

static query_map
send_requests(const host_users_map &,
              buffer_list &,
              failure_map &);

static host_users_map
parse_user_request(const json::object &device_keys);

static void
handle_failures(const failure_map &,
                json::stack::object &);

static m::resource::response
post__keys_query(client &client,
                 const m::resource::request &request);

mapi::header
IRCD_MODULE
{
	"Client 14.11.5.2 :Key management API"
};

ircd::m::resource
query_resource
{
	"/_matrix/client/r0/keys/query",
	{
		"(14.11.5.2.2) Keys query",
	}
};

m::resource::method
method_post
{
	query_resource, "POST", post__keys_query,
	{
		method_post.REQUIRES_AUTH
	}
};

conf::item<milliseconds>
query_timeout_default
{
	{ "name",     "ircd.client.keys.query.timeout.default" },
	{ "default",  10000L                                   },
};

conf::item<milliseconds>
query_timeout_min
{
	{ "name",     "ircd.client.keys.query.timeout.min" },
	{ "default",  5000L                                },
};

conf::item<milliseconds>
query_timeout_max
{
	{ "name",     "ircd.client.keys.query.timeout.max" },
	{ "default",  20000L                               },
};

conf::item<size_t>
query_limit
{
	{ "name",     "ircd.client.keys.query.limit" },
	{ "default",  4096L                          },
};

m::resource::response
post__keys_query(client &client,
                 const m::resource::request &request)
{
	const milliseconds
	timeout_min{query_timeout_min},
	timeout_max{query_timeout_max},
	timeout_default{query_timeout_default},
	timeout
	{
		std::clamp(request.get("timeout", timeout_default), timeout_min, timeout_max)
	};

	const json::string token
	{
		request["token"]
	};

	const m::event::idx since
	{
		m::sync::sequence(m::sync::make_since(token))
	};

	const json::object &request_keys
	{
		request.at("device_keys")
	};

	const host_users_map map
	{
		parse_user_request(request_keys)
	};

	buffer_list buffers;
	failure_map failures;
	query_map queries
	{
		send_requests(map, buffers, failures)
	};

	auto responses
	{
		ctx::when_all(begin(queries), end(queries), []
		(auto &it) -> m::fed::user::keys::query &
		{
			return it->second;
		})
	};

	const bool all_good
	{
		responses.wait_until(now<system_point>() + timeout, std::nothrow)
	};

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

	handle_responses(request, map, queries, failures, top);
	handle_failures(failures, top);
	return response;
}

void
handle_failures(const failure_map &failures,
                json::stack::object &out)
{
	json::stack::object response_failures
	{
		out, "failures"
	};

	for(const auto &[remote, eptr] : failures)
		json::stack::member
		{
			response_failures, remote, what(eptr)
		};
}

host_users_map
parse_user_request(const json::object &device_keys)
{
	host_users_map ret;
	for(const auto &member : device_keys)
	{
		const m::user::id &user_id(member.first);
		const json::array &device_ids(member.second);
		const string_view &host(user_id.host());

		auto it(ret.lower_bound(host));
		if(it == end(ret) || it->first != host)
			it = ret.emplace_hint(it, host, user_devices_map{});

		user_devices_map &users(it->second);
		{
			auto it(users.lower_bound(user_id));
			if(it == end(users) || it->first != user_id)
				it = users.emplace_hint(it, user_id, json::array{});

			if(!empty(device_ids))
				it->second = device_ids;
		}
	}

	return ret;
}

query_map
send_requests(const host_users_map &hosts,
              buffer_list &buffers,
              failure_map &failures)
{
	query_map ret;
	for(const auto &[remote, user_devices] : hosts)
		if(likely(!my_host(remote)))
			send_request(remote, user_devices, failures, buffers, ret);

	return ret;
}

bool
send_request(const string_view &remote,
             const user_devices_map &queries,
             failure_map &failures,
             buffer_list &buffers,
             query_map &ret)
try
{
	static const size_t buffer_unit_size
	{
		m::user::id::MAX_SIZE + 1     // 256
		+ 128                         // device_id
	};

	const size_t buffer_size
	{
		8_KiB + // headers
		buffer_unit_size * std::min(queries.size(), size_t(query_limit))
	};

	const auto &buffer
	{
		buffers.emplace_back(buffer_size)
	};

	m::fed::user::keys::query::opts opts;
	opts.remote = remote;
	ret.emplace
	(
		std::piecewise_construct,
		std::forward_as_tuple(remote),
		std::forward_as_tuple(queries, buffer, std::move(opts))
	);

	return true;
}
catch(const ctx::interrupted &e)
{
	throw;
}
catch(const std::exception &e)
{
	failures.emplace(remote, std::current_exception());
	log::derror
	{
		m::log, "user keys query to %s :%s",
		remote,
		e.what()
	};

	return false;
}

void
handle_responses(const m::resource::request &request,
                 const host_users_map &map,
                 query_map &queries,
                 failure_map &failures,
                 json::stack::object &out)
{
	static const user_devices_map empty;

	const auto it
	{
		map.find(origin(m::my()))
	};

	const user_devices_map &self
	{
		it != end(map)? it->second: empty
	};

	handle_errors(request, queries, failures);
	handle_device_keys(request, self, queries, failures, out);
	handle_cross_keys(request, self, queries, failures, out, "master_keys");
	handle_cross_keys(request, self, queries, failures, out, "self_signing_keys");
	handle_cross_keys(request, self, queries, failures, out, "user_signing_keys");
}

void
handle_errors(const m::resource::request &request,
              query_map &queries,
              failure_map &failures)
{
	auto it(begin(queries));
	while(it != end(queries))
	{
		const auto &[remote, query] {*it};
		if(query.eptr())
		{
			failures.emplace(remote, query.eptr());
			it = queries.erase(it);
		}
		else ++it;
	}
}

void
handle_device_keys(const m::resource::request &request,
                   const user_devices_map &self,
                   query_map &queries,
                   failure_map &failures,
                   json::stack::object &out)
{
	json::stack::object object
	{
		out, "device_keys"
	};

	// local handle
	for(const auto &[user_id, device_ids] : self)
	{
		const m::user::keys keys
		{
			user_id
		};

		json::stack::object user_object
		{
			object, user_id
		};

		if(empty(json::array(device_ids)))
		{
			const m::user::devices devices
			{
				user_id
			};

			devices.for_each([&user_object, &keys]
			(const auto &, const string_view &device_id)
			{
				json::stack::object device_object
				{
					user_object, device_id
				};

				keys.device(device_object, device_id);
			});
		}
		else for(const json::string device_id : json::array(device_ids))
		{
			json::stack::object device_object
			{
				user_object, device_id
			};

			keys.device(device_object, device_id);
		}
	}

	// remote handle
	for(const auto &[remote, query] : queries) try
	{
		const json::object response
		{
			query.in.content
		};

		const json::object &device_keys
		{
			response["device_keys"]
		};

		for(const auto &[user_id, device_keys] : device_keys)
		{
			if(m::user::id(user_id).host() != remote)
				continue;

			json::stack::object user_object
			{
				object, user_id
			};

			for(const auto &[device_id, keys] : json::object(device_keys))
				json::stack::member
				{
					user_object, device_id, keys
				};
		}
	}
	catch(const ctx::interrupted &)
	{
		throw;
	}
	catch(const std::exception &e)
	{
		failures.emplace(remote, std::current_exception());
		log::derror
		{
			m::log, "Processing device_keys response from '%s' :%s",
			remote,
			e.what(),
		};
	}
}

static std::tuple<string_view, bool>
translate_cross_type(const string_view &name)
{
	bool match_user;
	string_view cross_type;
	switch(match_user = false; hash(name))
	{
		case "master_keys"_:
			cross_type = "ircd.cross_signing.master";
			break;

		case "self_signing_keys"_:
			cross_type = "ircd.cross_signing.self";
			break;

		case "user_signing_keys"_:
			cross_type = "ircd.cross_signing.user";
			match_user = true;
			break;
	};

	assert(cross_type);
	return
	{
		cross_type, match_user
	};
}

void
handle_cross_keys(const m::resource::request &request,
                  const user_devices_map &self,
                  query_map &queries,
                  failure_map &failures,
                  json::stack::object &out_,
                  const string_view &name)
{
	const auto &[cross_type, match_user]
	{
		translate_cross_type(name)
	};

	json::stack::object out
	{
		out_, name
	};

	// local handle
	for(const auto &[user_id, device_ids] : self)
	{
		if(match_user && request.user_id != user_id)
			continue;

		const m::user::keys keys
		{
			user_id
		};

		if(!keys.has_cross(cross_type))
			continue;

		json::stack::object user_object
		{
			out, user_id
		};

		keys.cross(user_object, cross_type);
	}

	// remote handle
	for(auto &[remote, query] : queries) try
	{
		if(match_user && request.user_id.host() != remote)
			continue;

		const json::object response
		{
			query.in.content
		};

		const json::object &object
		{
			response[name]
		};

		for(const auto &[user_id, keys] : object)
		{
			if(m::user::id(user_id).host() != remote)
				continue;

			if(match_user && request.user_id != user_id)
				continue;

			json::stack::member
			{
				out, user_id, json::object
				{
					keys
				}
			};
		}
	}
	catch(const ctx::interrupted &)
	{
		throw;
	}
	catch(const std::exception &e)
	{
		failures.emplace(remote, std::current_exception());
		log::derror
		{
			m::log, "Processing %s response from '%s' :%s",
			name,
			remote,
			e.what(),
		};
	}
}
