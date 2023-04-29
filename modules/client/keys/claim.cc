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
	using user_devices_map = std::map<m::user::id, json::object>;
	using host_users_map = std::map<string_view, user_devices_map>;
	using query_map = std::map<string_view, m::fed::user::keys::claim>;
	using failure_map = std::map<string_view, std::exception_ptr, std::less<>>;
	using buffer_list = std::vector<unique_buffer<mutable_buffer>>;
}

static host_users_map
parse_user_request(const json::object &device_keys);

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

static void
recv_response(const string_view &,
              m::fed::user::keys::claim &,
              failure_map &,
              json::stack::object &,
              const system_point &);

static void
recv_responses(const host_users_map &,
               query_map &,
               failure_map &,
               json::stack::object &,
               const system_point &);

static void
handle_failures(const failure_map &,
                json::stack::object &);

static m::resource::response
post__keys_claim(client &client,
                 const m::resource::request &request);

mapi::header
IRCD_MODULE
{
	"Client 14.11.5.2 :Key management API"
};

ircd::m::resource
claim_resource
{
	"/_matrix/client/r0/keys/claim",
	{
		"(14.11.5.2.2) Keys claim",
	}
};

m::resource::method
method_post
{
	claim_resource, "POST", post__keys_claim,
	{
		method_post.REQUIRES_AUTH
	}
};

conf::item<milliseconds>
claim_timeout_default
{
	{ "name",     "ircd.client.keys.claim.timeout.default" },
	{ "default",  20000L                                   },
};

conf::item<milliseconds>
claim_timeout_min
{
	{ "name",     "ircd.client.keys.claim.timeout.min" },
	{ "default",  5000L                                },
};

conf::item<milliseconds>
claim_timeout_max
{
	{ "name",     "ircd.client.keys.claim.timeout.max" },
	{ "default",  30000L                               },
};

conf::item<size_t>
claim_limit
{
	{ "name",     "ircd.client.keys.claim.limit" },
	{ "default",  4096L                          },
};

m::resource::response
post__keys_claim(client &client,
                 const m::resource::request &request)
{
	const milliseconds
	timeout_min{claim_timeout_min},
	timeout_max{claim_timeout_max},
	timeout_default{claim_timeout_default},
	timeout
	{
		std::clamp(request.get("timeout", timeout_default), timeout_min, timeout_max)
	};

	const json::object &one_time_keys
	{
		request.at("one_time_keys")
	};

	const host_users_map map
	{
		parse_user_request(one_time_keys)
	};

	buffer_list buffers;
	failure_map failures;
	query_map queries
	{
		send_requests(map, buffers, failures)
	};

	const system_point timedout
	{
		ircd::now<system_point>() + timeout
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

	recv_responses(map, queries, failures, top, timedout);
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

	for(const auto &[hostname, eptr] : failures)
		json::stack::member
		{
			response_failures, hostname, what(eptr)
		};
}

void
recv_responses(const host_users_map &map,
               query_map &queries,
               failure_map &failures,
               json::stack::object &out,
               const system_point &timeout)
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

	json::stack::object one_time_keys
	{
		out, "one_time_keys"
	};

	// local handle
	for(const auto &[user_id, reqs] : self)
	{
		const m::user::keys keys
		{
			user_id
		};

		json::stack::object user_object
		{
			one_time_keys, user_id
		};

		for(const auto &[device_id, algorithm] : json::object(reqs))
		{
			json::stack::object device_object
			{
				user_object, device_id
			};

			keys.claim(device_object, device_id, json::string(algorithm));
		}
	}

	// remote handle
	while(!queries.empty())
	{
		auto next
		{
			ctx::when_any(begin(queries), end(queries), []
			(auto &it) -> m::fed::user::keys::claim &
			{
				return it->second;
			})
		};

		const bool ok
		{
			next.wait_until(timeout, std::nothrow)
		};

		const auto it(next.get());
		const unwind remove{[&queries, &it]
		{
			queries.erase(it);
		}};

		auto &[remote, request] {*it};
		recv_response(remote, request, failures, one_time_keys, timeout);
	}
}

void
recv_response(const string_view &remote,
              m::fed::user::keys::claim &request,
              failure_map &failures,
              json::stack::object &object,
              const system_point &timeout)
try
{
	const auto code
	{
		request.get_until(timeout)
	};

	const json::object response
	{
		request
	};

	const json::object &one_time_keys
	{
		response["one_time_keys"]
	};

	for(const auto &[user_id, keys] : one_time_keys)
	{
		if(m::user::id(user_id).host() != remote)
			continue;

		json::stack::member
		{
			object, user_id, json::object
			{
				keys
			}
		};
	}
}
catch(const std::exception &e)
{
	log::derror
	{
		m::log, "user keys claim from %s :%s",
		remote,
		e.what()
	};

	failures.emplace(remote, std::current_exception());
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
		+ 128                         // algorithm
	};

	static_assert(math::is_pow2(buffer_unit_size));
	const size_t buffer_size
	{
		8_KiB + // headers
		buffer_unit_size * std::min(queries.size(), size_t(claim_limit))
	};

	const auto &buffer
	{
		buffers.emplace_back(buffer_size)
	};

	m::fed::user::keys::claim::opts opts;
	opts.remote = remote;
	ret.emplace
	(
		std::piecewise_construct,
		std::forward_as_tuple(remote),
		std::forward_as_tuple(queries, buffer, std::move(opts))
	);

	return true;
}
catch(const std::exception &e)
{
	log::derror
	{
		m::log, "user keys claim to %s for %zu users :%s",
		remote,
		queries.size(),
		e.what()
	};

	failures.emplace(remote, std::current_exception());
	return false;
}

host_users_map
parse_user_request(const json::object &one_time_keys)
{
	host_users_map ret;
	for(const auto &[user_id, devices] : one_time_keys)
	{
		const string_view &host
		{
			m::user::id{user_id}.host()
		};

		auto it(ret.lower_bound(host));
		if(it == end(ret) || it->first != host)
			it = ret.emplace_hint(it, host, user_devices_map{});

		user_devices_map &users(it->second);
		{
			auto it(users.lower_bound(user_id));
			if(it == end(users) || it->first != user_id)
				it = users.emplace_hint(it, user_id, json::object{});

			it->second = json::object{devices};
		}
	}

	return ret;
}
