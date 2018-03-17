// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd::m;
using namespace ircd;

static bool verify(const m::keys &) noexcept;
static bool get_local(const string_view &server_name, const m::keys::closure &);
static event::id::buf set_keys(const m::keys &);

mapi::header
IRCD_MODULE
{
	"Federation 2.3 :Retrieving Server Keys"
};

extern "C" void
get__keys(const string_view &server_name,
          const m::keys::closure &closure)
{
	assert(!server_name.empty());

	if(get_local(server_name, closure))
		return;

	if(server_name == my_host())
		throw m::NOT_FOUND
		{
			"keys for '%s' (that's myself) not found", server_name
		};

	m::log.debug("Keys for %s not cached; querying network...",
	             server_name);

	//TODO: XXX
	const unique_buffer<mutable_buffer> buffer
	{
		32_KiB
	};

	const mutable_buffer out_head
	{
		data(buffer), 8_KiB
	};

	using buffer::size;
	const mutable_buffer in
	{
		data(out_head) + size(out_head), size(buffer) - size(out_head)
	};

	m::request request
	{
		my_host(), server_name, "GET", "/_matrix/key/v2/server/", {}
	};

	const auto request_head
	{
		request(out_head)
	};

	const net::hostport host
	{
		server_name
	};

	server::request tag
	{
		host, { request_head }, { in }
	};

	//TODO: conf
	if(tag.wait(seconds(20)) == ctx::future_status::timeout)
	{
		cancel(tag);
		throw m::error
		{
			http::REQUEST_TIMEOUT, "M_TIMEOUT",
			"Failed to fetch keys for '%s' in time",
			server_name
		};
	}

	const http::code status
	{
		tag.get()
	};

	const json::object response
	{
		tag.in.content
	};

	const m::keys &keys
	{
		response
	};

	if(!verify(keys))
		throw m::error
		{
			http::UNAUTHORIZED, "M_INVALID_SIGNATURE",
			"Failed to verify keys for '%s'",
			server_name
		};

	m::log.debug("Verified keys from '%s'",
	             server_name);

	set_keys(keys);
	closure(keys);
}

extern "C" void
query__keys(const string_view &server_name,
            const string_view &key_id,
            const string_view &query_server,
            const m::keys::closure &closure)
try
{
	assert(!server_name.empty());
	assert(!query_server.empty());

	thread_local char url_buf[2_KiB];
	thread_local char key_id_buf[1_KiB];
	thread_local char server_name_buf[1_KiB];
	const string_view url{fmt::sprintf
	{
		url_buf, "/_matrix/key/v2/query/%s/%s/",
		url::encode(server_name, server_name_buf),
		url::encode(key_id, key_id_buf)
	}};

	// This buffer will hold the HTTP request and response
	//TODO: XXX
	const unique_buffer<mutable_buffer> buffer
	{
		32_KiB
	};

	m::request request
	{
		my_host(), server_name, "GET", url, {}
	};

	// Generates the HTTP request head into the front of buffer
	const string_view head
	{
		request(buffer)
	};

	// Partition the remainder of buffer for the response data
	using buffer::size;
	assert(size(head) < size(buffer) / 2);
	const mutable_buffer in
	{
		data(buffer) + size(head), size(buffer) - size(head)
	};

	// The request to the remote is transmitted
	server::request tag
	{
		server_name, { head }, { in }
	};

	if(tag.wait(seconds(30)) == ctx::future_status::timeout)
		throw m::error
		{
			http::REQUEST_TIMEOUT, "M_TIMEOUT",
			"Failed to fetch keys for '%s' from '%s' in time",
			server_name,
			query_server
		};

	// The response from the remote is received
	const auto code
	{
		tag.get()
	};

	// The content received is here
	const json::object response
	{
		tag.in.content
	};

	// This parses the content for our key.
	const json::array &keys
	{
		response.at("server_keys")
	};

	m::log.debug("Fetched %zu candidate keys seeking '%s' for '%s' from '%s'",
	             keys.count(),
	             empty(key_id)? "*" : key_id,
	             server_name,
	             query_server);

	bool ret{false};
	for(auto it(begin(keys)); it != end(keys); ++it)
	{
		const m::keys &keys{*it};
		const auto &_server_name
		{
			at<"server_name"_>(keys)
		};

		if(!verify(keys))
			throw m::error
			{
				http::UNAUTHORIZED, "M_INVALID_SIGNATURE",
				"Failed to verify keys for '%s' from '%s'",
				_server_name,
				query_server
			};

		m::log.debug("Verified keys for '%s' from '%s'",
		             _server_name,
		             query_server);

		set_keys(keys);
		const json::object vks{json::get<"verify_keys"_>(keys)};
		if(_server_name == server_name)
		{
			closure(keys);
			ret = true;
		}
	}

	if(!ret)
		throw m::NOT_FOUND
		{
			"Failed to get any keys for '%s' from '%s' (got %zu total keys otherwise)",
			server_name,
			query_server,
			keys.count()
		};
}
catch(const json::not_found &e)
{
	throw m::NOT_FOUND
	{
		"Failed to find key '%s' for '%s' when querying '%s': %s",
		key_id,
		server_name,
		query_server,
		e.what()
	};
}

event::id::buf
set_keys(const m::keys &keys)
{
	const auto &server_name
	{
		unquote(at<"server_name"_>(keys))
	};

	const m::node::id::buf node_id
	{
		"", server_name
	};

	const m::node::room node_room
	{
		node_id
	};

	if(!exists(node_room.room_id))
		create(node_room, m::me.user_id);

	const json::strung derp
	{
		keys
	};

	return send(node_room, m::me.user_id, "ircd.keys", "", json::object{derp});
}

bool
get_local(const string_view &server_name,
          const m::keys::closure &closure)
{
	const node::id::buf node_id
	{
		"", server_name
	};

	const m::node::room node_room
	{
		node_id
	};

	return node_room.get(std::nothrow, "ircd.keys", "", [&closure]
	(const m::event &event)
	{
		closure(json::get<"content"_>(event));
	});
}

bool
verify(const m::keys &keys)
noexcept try
{
	const auto &valid_until_ts
	{
		at<"valid_until_ts"_>(keys)
	};

	if(valid_until_ts < ircd::time<milliseconds>())
		throw ircd::error("Key was valid until %s", timestr(valid_until_ts));

	const json::object &verify_keys
	{
		at<"verify_keys"_>(keys)
	};

	const string_view &key_id
	{
		begin(verify_keys)->first
	};

	const json::object &key
	{
		begin(verify_keys)->second
	};

	const ed25519::pk pk
	{
		[&key](auto &pk)
		{
			b64decode(pk, unquote(key.at("key")));
		}
	};

	const json::object &signatures
	{
		at<"signatures"_>(keys)
	};

	const string_view &server_name
	{
		unquote(at<"server_name"_>(keys))
	};

	const json::object &server_signatures
	{
		signatures.at(server_name)
	};

	const ed25519::sig sig{[&server_signatures, &key_id](auto &sig)
	{
		b64decode(sig, unquote(server_signatures.at(key_id)));
	}};

	///TODO: XXX
	m::keys copy{keys};
	at<"signatures"_>(copy) = string_view{};
	const json::strung preimage{copy};
	return pk.verify(const_buffer{preimage}, sig);
}
catch(const std::exception &e)
{
	m::log.error("key verification for '%s' failed: %s",
	             json::get<"server_name"_>(keys, "<no server name>"_sv),
	             e.what());

	return false;
}

static void
create_my_key(const m::event &)
{
	const json::strung verify_keys
	{
		json::members
		{{
			string_view{self::public_key_id},
			{
				{ "key", self::public_key_b64 }
			}
		}}
	};

	m::keys my_key;
	json::get<"verify_keys"_>(my_key) = verify_keys;
	json::get<"server_name"_>(my_key) = my_host();
	json::get<"old_verify_keys"_>(my_key) = "{}";
	json::get<"valid_until_ts"_>(my_key) = ircd::time<milliseconds>() + duration_cast<milliseconds>(hours(2160)).count();

	const json::members tlsfps
	{
		{ "sha256", self::tls_cert_der_sha256_b64 }
	};

	const json::value tlsfp[1]
	{
		{ tlsfps }
	};

	const json::strung tls_fingerprints{json::value
	{
		tlsfp, 1
	}};

	json::get<"tls_fingerprints"_>(my_key) = tls_fingerprints;

	const auto presig
	{
		json::strung(my_key)
	};

	const ed25519::sig sig
	{
		self::secret_key.sign(const_buffer{presig})
	};

	static char signature[256];
	const json::strung signatures{json::members
	{
		{ my_host(), json::members
		{
			{ string_view{self::public_key_id}, b64encode_unpadded(signature, sig) }
		}}
	}};

	json::get<"signatures"_>(my_key) = signatures;
	set_keys(my_key);
}

const m::hook
create_my_key_hook
{
	create_my_key,
	{
		{ "_site",     "vm.notify"           },
		{ "room_id",   m::my_node.room_id()  },
		{ "type",      "m.room.create"       },
	}
};
