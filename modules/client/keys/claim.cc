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

using user_devices_map = std::map<m::user::id, json::object>;
using host_users_map = std::map<string_view, user_devices_map>;
using query_map = std::map<string_view, m::v1::user::keys::claim>;
using failure_map = std::map<string_view, std::exception_ptr, std::less<>>;
using buffer_list = std::vector<unique_buffer<mutable_buffer>>;

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
              m::v1::user::keys::claim &,
              failure_map &,
              json::stack::object &,
              const steady_point &);

static void
recv_responses(query_map &,
               failure_map &,
               json::stack::object &,
               const milliseconds &);

static void
handle_failures(const failure_map &,
                json::stack::object &);

static resource::response
post__keys_claim(client &client,
                 const resource::request &request);

mapi::header
IRCD_MODULE
{
	"Client 14.11.5.2 :Key management API"
};

ircd::resource
claim_resource
{
	"/_matrix/client/r0/keys/claim",
	{
		"(14.11.5.2.2) Keys claim",
	}
};

ircd::resource
claim_resource__unstable
{
	"/_matrix/client/unstable/keys/claim",
	{
		"(14.11.5.2.2) Keys claim",
	}
};

resource::method
method_post
{
	claim_resource, "POST", post__keys_claim,
	{
		method_post.REQUIRES_AUTH
	}
};

resource::method
method_post__unstable
{
	claim_resource__unstable, "POST", post__keys_claim,
	{
		method_post.REQUIRES_AUTH
	}
};

conf::item<milliseconds>
claim_timeout_default
{
	{ "name",     "ircd.client.keys.claim.timeout.default" },
	{ "default",  10000L                                   },
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
	{ "default",  20000L                               },
};

resource::response
post__keys_claim(client &client,
                 const resource::request &request)
{
	const milliseconds timeout{[&request]
	{
		const milliseconds _default(claim_timeout_default);
		milliseconds ret(request.get("timeout", _default));
		ret = std::max(ret, milliseconds(claim_timeout_min));
		ret = std::min(ret, milliseconds(claim_timeout_max));
		return ret;
	}()};

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

	recv_responses(queries, failures, top, timeout);
	handle_failures(failures, top);
	return {};
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
recv_responses(query_map &queries,
               failure_map &failures,
               json::stack::object &out,
               const milliseconds &timeout)
{
	const steady_point timedout
	{
		ircd::now<steady_point>() + timeout
	};

	json::stack::object one_time_keys
	{
		out, "one_time_keys"
	};

	for(auto &[remote, request] : queries)
	{
		assert(!failures.count(remote));
		if(failures.count(remote))
			continue;

		recv_response(remote, request, failures, one_time_keys, timedout);
	}
}

void
recv_response(const string_view &remote,
              m::v1::user::keys::claim &request,
              failure_map &failures,
              json::stack::object &object,
              const steady_point &timeout)
try
{
	request.wait_until(timeout);
	const auto code
	{
		request.get()
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
		json::stack::member
		{
			object, m::user::id{user_id}, json::object{keys}
		};
}
catch(const std::exception &e)
{
	log::error
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
	m::v1::user::keys::claim::opts opts;
	opts.remote = remote;
	const auto &buffer
	{
		buffers.emplace_back(8_KiB)
	};

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
	log::error
	{
		m::log, "user keys claim to %s :%s",
		remote,
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
