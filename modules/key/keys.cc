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

extern "C" bool verify__keys(const m::keys &) noexcept;
static bool cache_get(const string_view &server, const string_view &key_id, const m::keys::closure &);
static size_t cache_set(const json::object &);

extern "C" void get__keys(const string_view &server, const string_view &key_id, const m::keys::closure &);
extern "C" bool query__keys(const string_view &query_server, const m::keys::queries &, const m::keys::closure_bool &);

mapi::header
IRCD_MODULE
{
	"Federation 2.3 :Retrieving Server Keys"
};

conf::item<milliseconds>
query_keys_timeout
{
	{ "name",     "ircd.key.keys.query.timeout" },
	{ "default",  20000L                        }
};

extern "C" bool
query__keys(const string_view &query_server,
            const m::keys::queries &queries,
            const m::keys::closure_bool &closure)
try
{
	assert(!query_server.empty());

	m::v1::key::opts opts;
	opts.remote = net::hostport{query_server};
	opts.dynamic = true;
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	m::v1::key::query request
	{
		queries, buf, std::move(opts)
	};

	const milliseconds timeout(query_keys_timeout);
	request.wait(timeout);
	const auto &code
	{
		request.get()
	};

	const json::array &response
	{
		request
	};

	for(const json::object &k : response)
	{
		const m::keys &key{k};
		if(!verify__keys(key))
		{
			log::derror
			{
				"Failed to verify keys for '%s' from '%s'",
				at<"server_name"_>(key),
				query_server
			};

			continue;
		}

		log::debug
		{
			m::log, "Verified keys for '%s' from '%s'",
			at<"server_name"_>(key),
			query_server
		};

		if(!closure(k))
			return false;
	}

	return true;
}
catch(const ctx::timeout &e)
{
	throw m::error
	{
		http::REQUEST_TIMEOUT, "M_TIMEOUT",
		"Failed to query keys from '%s' in time",
		query_server
	};
}

conf::item<milliseconds>
get_keys_timeout
{
	{ "name",     "ircd.key.keys.get.timeout" },
	{ "default",  20000L                       }
};

void
get__keys(const string_view &server_name,
          const string_view &key_id,
          const m::keys::closure &closure)
try
{
	assert(!server_name.empty());

	if(cache_get(server_name, key_id, closure))
		return;

	if(server_name == my_host())
		throw m::NOT_FOUND
		{
			"keys for '%s' (that's myself) not found", server_name
		};

	log::debug
	{
		m::log, "Keys for %s not cached; querying network...", server_name
	};

	m::v1::key::opts opts;
	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	m::v1::key::keys request
	{
		server_name, buf, std::move(opts)
	};

	const milliseconds timeout(get_keys_timeout);
	request.wait(timeout);
	const auto &status
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	const json::object &keys
	{
		response
	};

	if(!verify__keys(keys)) throw m::error
	{
		http::UNAUTHORIZED, "M_INVALID_SIGNATURE",
		"Failed to verify keys for '%s'",
		server_name
	};

	log::debug
	{
		m::log, "Verified keys from '%s'", server_name
	};

	cache_set(keys);
	closure(keys);
}
catch(const ctx::timeout &e)
{
	throw m::error
	{
		http::REQUEST_TIMEOUT, "M_TIMEOUT",
		"Failed to fetch keys for '%s' in time",
		server_name
	};
}

size_t
cache_set(const json::object &keys)
{
	const auto &server_name
	{
		unquote(keys.at("server_name"))
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

	const json::object &vks
	{
		keys.at("verify_keys")
	};

	size_t ret{0};
	for(const auto &member : vks)
	{
		if(ret > 16)
			return ret;

		const auto &key_id(unquote(member.first));
		send(node_room, m::me.user_id, "ircd.key", key_id, keys);
		++ret;
	}

	return ret;
}

bool
cache_get(const string_view &server_name,
          const string_view &key_id,
          const m::keys::closure &closure)
{
	const m::node::id::buf node_id
	{
		"", server_name
	};

	const m::node::room node_room
	{
		node_id
	};

	const auto reclosure{[&closure]
	(const m::event &event)
	{
		closure(json::get<"content"_>(event));
	}};

	// Without a key_id we search for the most recent key; note this is not
	// the same as making a state_key="" query, as that would be an actual
	// ircd.key entry without an id (which shouldn't exist).
	return !key_id?
		node_room.get(std::nothrow, "ircd.key", reclosure):
		node_room.get(std::nothrow, "ircd.key", key_id, reclosure);
}

bool
verify__keys(const m::keys &keys)
noexcept try
{
	const auto &valid_until_ts
	{
		at<"valid_until_ts"_>(keys)
	};

	if(valid_until_ts < ircd::time<milliseconds>())
		throw ircd::error
		{
			"Key was valid until %s", timestr(valid_until_ts)
		};

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

	m::keys copy{keys};
	at<"signatures"_>(copy) = string_view{};

	thread_local char buf[4096];
	const const_buffer preimage
	{
		json::stringify(mutable_buffer{buf}, copy)
	};

	return pk.verify(preimage, sig);
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "key verification for '%s' failed: %s",
		json::get<"server_name"_>(keys, "<no server name>"_sv),
		e.what()
	};

	return false;
}

static void
create_my_key(const m::event &)
{
	const json::strung verify_keys{json::members
	{
		{ string_view{m::self::public_key_id},
		{
			{ "key", m::self::public_key_b64 }
		}}
	}};

	const json::members tlsfps
	{
		{ "sha256", m::self::tls_cert_der_sha256_b64 }
	};

	const json::value tlsfp[1]
	{
		{ tlsfps }
	};

	const json::strung tls_fingerprints{json::value
	{
		tlsfp, 1
	}};

	m::keys my_key;
	json::get<"server_name"_>(my_key) = my_host();
	json::get<"old_verify_keys"_>(my_key) = "{}";
	json::get<"valid_until_ts"_>(my_key) = ircd::time<milliseconds>() + duration_cast<milliseconds>(hours(2160)).count();
	json::get<"verify_keys"_>(my_key) = verify_keys;
	json::get<"tls_fingerprints"_>(my_key) = tls_fingerprints;

	const json::strung presig
	{
		my_key
	};

	const ed25519::sig sig
	{
		m::self::secret_key.sign(const_buffer{presig})
	};

	char signature[256];
	const json::strung signatures{json::members
	{
		{ my_host(),
		{
			{ string_view{m::self::public_key_id}, b64encode_unpadded(signature, sig) }
		}}
	}};

	json::get<"signatures"_>(my_key) = signatures;
	cache_set(json::strung{my_key});
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
