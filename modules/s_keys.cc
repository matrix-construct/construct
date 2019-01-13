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

static bool cache_get(const string_view &server, const string_view &key_id, const m::keys::closure &);
static size_t cache_set(const json::object &);

extern "C" bool verify__keys(const m::keys &) noexcept;
extern "C" void get__keys(const string_view &server, const string_view &key_id, const m::keys::closure &);
extern "C" bool query__keys(const string_view &query_server, const m::keys::queries &, const m::keys::closure_bool &);

extern "C" void create_my_key(const m::event &, m::vm::eval &);
static void init_my_ed25519();
static void init_my_tls_crt();
extern "C" void init_my_keys();

mapi::header
IRCD_MODULE
{
	"Server keys"
};

void
init_my_keys()
{
	init_my_ed25519();
	init_my_tls_crt();
}

conf::item<std::string>
tls_key_dir
{
	{ "name",     "ircd.keys.tls_key_dir"  },
	{ "default",  fs::cwd()                }
};

void
init_my_tls_crt()
{
	if(!m::self::origin)
		throw error
		{
			"The m::self::origin must be set to init my ed25519 key."
		};

	const std::string private_key_path_parts[]
	{
		std::string{tls_key_dir},
		m::self::origin + ".crt.key",
	};

	const std::string public_key_path_parts[]
	{
		std::string{tls_key_dir},
		m::self::origin + ".crt.key.pub",
	};

	const std::string dhparam_path_parts[]
	{
		std::string{tls_key_dir},
		m::self::origin + ".crt.dh",
	};

	const std::string certificate_path_parts[]
	{
		std::string{tls_key_dir},
		m::self::origin + ".crt",
	};

	const std::string private_key_file
	{
		fs::make_path(private_key_path_parts)
	};

	const std::string public_key_file
	{
		fs::make_path(public_key_path_parts)
	};

	const std::string cert_file
	{
		fs::make_path(certificate_path_parts)
	};

	if(!fs::exists(private_key_file))
	{
		log::warning
		{
			"Failed to find certificate private key @ `%s'; creating...",
			private_key_file
		};

		openssl::genrsa(private_key_file, public_key_file);
	}

/*
	const std::string dhparam_file
	{
		fs::make_path(dhparam_path_parts)
	};

	if(!fs::exists(dhparam_file))
	{
		log::warning
		{
			"Failed to find dhparam file @ `%s'; creating; "
			"this will take about 2 to 5 minutes...",
			dhparam_file
		};

		openssl::gendh(dhparam_file);
	}
*/

	const json::object config{};
	if(!fs::exists(cert_file))
	{
		const json::object &certificate
		{
			config.get("certificate")
		};

		const json::object &self_
		{
			certificate.get(m::self::origin)
		};

		std::string subject
		{
			self_.get("subject")
		};

		if(!subject)
			subject = json::strung{json::members
			{
				{ "CN", m::self::origin }
			}};

		log::warning
		{
			"Failed to find SSL certificate @ `%s'; creating for '%s'...",
			cert_file,
			m::self::origin
		};

		const unique_buffer<mutable_buffer> buf
		{
			1_MiB
		};

		const json::strung opts{json::members
		{
			{ "private_key_pem_path",  private_key_file  },
			{ "public_key_pem_path",   public_key_file   },
			{ "subject",               subject           },
		}};

		const auto cert
		{
			openssl::genX509_rsa(buf, opts)
		};

		fs::overwrite(cert_file, cert);
	}

	const auto cert_pem
	{
		fs::read(cert_file)
	};

	const unique_buffer<mutable_buffer> der_buf
	{
		8_KiB
	};

	const auto cert_der
	{
		openssl::cert2d(der_buf, cert_pem)
	};

	const fixed_buffer<const_buffer, crh::sha256::digest_size> hash
	{
		sha256{cert_der}
	};

	m::self::tls_cert_der_sha256_b64 =
	{
		b64encode_unpadded(hash)
	};

	log::info
	{
		m::log, "Certificate `%s' :PEM %zu bytes; DER %zu bytes; sha256b64 %s",
		cert_file,
		cert_pem.size(),
		ircd::size(cert_der),
		m::self::tls_cert_der_sha256_b64
	};

	const unique_buffer<mutable_buffer> print_buf
	{
		8_KiB
	};

	log::info
	{
		m::log, "Certificate `%s' :%s",
		cert_file,
		openssl::print_subject(print_buf, cert_pem)
	};
}

conf::item<std::string>
ed25519_key_dir
{
	{ "name",     "ircd.keys.ed25519_key_dir"  },
	{ "default",  fs::cwd()                    }
};

void
init_my_ed25519()
{
	if(!m::self::origin)
		throw error
		{
			"The m::self::origin must be set to init my ed25519 key."
		};

	const std::string path_parts[]
	{
		std::string{ed25519_key_dir},
		m::self::origin + ".ed25519",
	};

	const std::string sk_file
	{
		fs::make_path(path_parts)
	};

	if(fs::exists(sk_file))
		log::info
		{
			m::log, "Using ed25519 secret key @ `%s'", sk_file
		};
	else
		log::notice
		{
			m::log, "Creating ed25519 secret key @ `%s'", sk_file
		};

	m::self::secret_key = ed25519::sk
	{
		sk_file, &m::self::public_key
	};

	m::self::public_key_b64 = b64encode_unpadded(m::self::public_key);
	const fixed_buffer<const_buffer, sha256::digest_size> hash
	{
		sha256{m::self::public_key}
	};

	const auto public_key_hash_b58
	{
		b58encode(hash)
	};

	static const auto trunc_size{8};
	m::self::public_key_id = fmt::snstringf
	{
		32, "ed25519:%s", trunc(public_key_hash_b58, trunc_size)
	};

	log::info
	{
		m::log, "Current key is '%s' and the public key is: %s",
		m::self::public_key_id,
		m::self::public_key_b64
	};
}

const m::hookfn<m::vm::eval &>
create_my_key_hook
{
	create_my_key,
	{
		{ "_site",     "vm.effect"           },
		{ "room_id",   m::my_node.room_id()  },
		{ "type",      "m.room.create"       },
	}
};

void
create_my_key(const m::event &,
              m::vm::eval &)
{
	const json::members verify_keys_
	{{
		string_view{m::self::public_key_id},
		{
			{ "key", m::self::public_key_b64 }
		}
	}};

	const json::members tlsfps
	{
		{ "sha256", m::self::tls_cert_der_sha256_b64 }
	};

	const json::value tlsfp[1]
	{
		{ tlsfps }
	};

	m::keys my_key;
	json::get<"server_name"_>(my_key) = my_host();
	json::get<"old_verify_keys"_>(my_key) = "{}";
	json::get<"valid_until_ts"_>(my_key) = ircd::time<milliseconds>() + duration_cast<milliseconds>(hours(2160)).count();

	const json::strung verify_keys{verify_keys_}; // must be on stack until my_keys serialized.
	json::get<"verify_keys"_>(my_key) = verify_keys;

	const json::strung tls_fingerprints{json::value{tlsfp, 1}}; // must be on stack until my_keys serialized.
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

//
// query
//

conf::item<milliseconds>
query_keys_timeout
{
	{ "name",     "ircd.keys.query.timeout" },
	{ "default",  20000L                    }
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
	{ "name",     "ircd.keys.get.timeout" },
	{ "default",  20000L                  }
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

size_t
cache_set(const json::object &keys)
{
	const auto &server_name
	{
		unquote(keys.at("server_name"))
	};

	const m::node::id::buf node_id
	{
		m::node::id::origin, server_name
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
		m::node::id::origin, server_name
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
