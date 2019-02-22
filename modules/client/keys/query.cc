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

using user_devices_map = std::map<m::user::id, json::array>;
using host_users_map = std::map<string_view, user_devices_map>;
using query_map = std::map<string_view, m::v1::user::keys::query>;
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
              m::v1::user::keys::query &,
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
post__keys_query(client &client,
                 const resource::request &request);

mapi::header
IRCD_MODULE
{
	"Client 14.11.5.2 :Key management API"
};

ircd::resource
query_resource
{
	"/_matrix/client/r0/keys/query",
	{
		"(14.11.5.2.2) Keys query",
	}
};

ircd::resource::redirect::permanent
query_resource__unstable
{
	"/_matrix/client/unstable/keys/query",
	"/_matrix/client/r0/keys/query",
	{
		"(14.11.5.2.2) Keys query",
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

resource::method
method_post
{
	query_resource, "POST", post__keys_query,
	{
		method_post.REQUIRES_AUTH
	}
};

resource::response
post__keys_query(client &client,
                 const resource::request &request)
{
	const milliseconds timeout{[&request]
	{
		const milliseconds _default(query_timeout_default);
		milliseconds ret(request.get("timeout", _default));
		ret = std::max(ret, milliseconds(query_timeout_min));
		ret = std::min(ret, milliseconds(query_timeout_max));
		return ret;
	}()};

	const auto &token
	{
		request["token"]
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

	for(const auto &p : failures)
	{
		const string_view &hostname(p.first);
		const std::exception_ptr &eptr(p.second);
		json::stack::member
		{
			response_failures, hostname, what(eptr)
		};
	}
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

	json::stack::object response_keys
	{
		out, "device_keys"
	};

	for(auto &pair : queries)
	{
		const auto &remote(pair.first);
		auto &request(pair.second);

		assert(!failures.count(remote));
		if(failures.count(remote))
			continue;

		recv_response(remote, request, failures, response_keys, timedout);
	}
}

void
recv_response(const string_view &remote,
              m::v1::user::keys::query &request,
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

	const json::object &response{request};
	const json::object &device_keys
	{
		response["device_keys"]
	};

	for(const auto &m : device_keys)
	{
		const m::user::id &user_id(m.first);
		const json::object &device_keys(m.second);
		json::stack::member
		{
			object, user_id, device_keys
		};
	}
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "user keys query from %s :%s",
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
	for(const auto &pair : hosts)
	{
		const string_view &remote(pair.first);
		const user_devices_map &user_devices(pair.second);
		send_request(remote, user_devices, failures, buffers, ret);
	}

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
	m::v1::user::keys::query::opts opts;
	opts.remote = remote;
	const auto &buffer
	{
		buffers.emplace_back(32_KiB)
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
		m::log, "user keys query to %s :%s",
		remote,
		e.what()
	};

	failures.emplace(remote, std::current_exception());
	return false;
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

	return std::move(ret);
}
