// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::mapi::header
IRCD_MODULE
{
	"Server keys"
	,ircd::m::self::init::keys
};

std::ostream &
IRCD_MODULE_EXPORT
ircd::m::pretty_oneline(std::ostream &s,
                        const m::keys &keys)
{
	s << json::get<"server_name"_>(keys)
	  << ' ';

	char smbuf[32];
	s << smalldate(smbuf, json::get<"valid_until_ts"_>(keys) / 1000L)
	  << " (" << json::get<"valid_until_ts"_>(keys) << ")"
	  << ' ';

	for(const json::object &fp : json::get<"tls_fingerprints"_>(keys))
	{
		s << "tls[ ";
		for(const auto &[digest, fingerprint] : fp)
			s << digest << ' ';
		s << "] ";
	}

	for(const auto &[domain, signature_] : json::get<"signatures"_>(keys))
	{
		s << "sig[ " << domain << ' ';
		for(const auto &[key_id, signature] : json::object(signature_))
			s << key_id << ' ';
		s << "] ";
	}

	for(const auto &[domain, verify_key_] : json::get<"verify_keys"_>(keys))
	{
		s << "key[ " << domain << ' ';
		for(const auto &[key_id, verify_key] : json::object(verify_key_))
			s << key_id << ' ';
		s << "] ";
	}

	return s;
}

std::ostream &
IRCD_MODULE_EXPORT
ircd::m::pretty(std::ostream &s,
                const m::keys &keys)
{
	s << std::setw(16) << std::right << "server name  "
	  << json::get<"server_name"_>(keys)
	  << '\n';

	char tmbuf[64];
	s << std::setw(16) << std::right << "valid until  "
	  << timef(tmbuf, json::get<"valid_until_ts"_>(keys) / 1000, ircd::localtime)
	  << " (" << json::get<"valid_until_ts"_>(keys) << ")"
	  << '\n';

	for(const json::object &fp : json::get<"tls_fingerprints"_>(keys))
		for(const auto &[digest, fingerprint] : fp)
			s << std::setw(16) << std::right << "[fingerprint]  "
			  << digest << ' ' << unquote(fingerprint)
			  << '\n';

	for(const auto &[domain, signature_] : json::get<"signatures"_>(keys))
		for(const auto &[key_id, signature] : json::object(signature_))
			s << std::setw(16) << std::right << "[signature]  "
			  << domain << ' '
			  << key_id << ' '
			  << unquote(signature) << '\n';

	for(const auto &[domain, verify_key_] : json::get<"verify_keys"_>(keys))
		for(const auto &[key_id, verify_key] : json::object(verify_key_))
			s << std::setw(16) << std::right << "[verify_key]  "
			  << domain << ' '
			  << key_id << ' '
			  << unquote(verify_key) << '\n';

	for(const auto &[domain, old_verify_key_] : json::get<"old_verify_keys"_>(keys))
		for(const auto &[key_id, old_verify_key] : json::object(old_verify_key_))
			s << std::setw(16) << std::right << "[old_verify_key]  "
			  << domain << ' '
			  << key_id << ' '
			  << unquote(old_verify_key) << '\n';

	return s;
}

bool
IRCD_MODULE_EXPORT
ircd::m::verify(const m::keys &keys,
                std::nothrow_t)
noexcept try
{
	verify(keys);
	return true;
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "key verification for '%s' failed :%s",
		json::get<"server_name"_>(keys, "<no server name>"_sv),
		e.what()
	};

	return false;
}

void
IRCD_MODULE_EXPORT
ircd::m::verify(const m::keys &keys)
{
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

	const ed25519::sig sig
	{
		[&server_signatures, &key_id](auto &sig)
		{
			b64decode(sig, unquote(server_signatures.at(key_id)));
		}
	};

	m::keys copy{keys};
	at<"signatures"_>(copy) = string_view{};

	thread_local char buf[16_KiB];
	const const_buffer preimage
	{
		json::stringify(mutable_buffer{buf}, copy)
	};

	if(!pk.verify(preimage, sig))
		throw m::error
		{
			http::UNAUTHORIZED, "M_INVALID_SIGNATURE",
			"Failed to verify signature for public key of '%s'",
			server_name
		};

	if(expired(keys))
		log::warning
		{
			m::log, "key '%s' for '%s' expired on %s.",
			key_id,
			json::get<"server_name"_>(keys, "<no server name>"_sv),
			timestr(at<"valid_until_ts"_>(keys) / 1000L),
		};
}

bool
IRCD_MODULE_EXPORT
ircd::m::expired(const m::keys &keys)
{
	const auto &valid_until_ts
	{
		at<"valid_until_ts"_>(keys)
	};

	return valid_until_ts < ircd::time<milliseconds>();
}

//
// query
//

namespace ircd::m
{
	extern conf::item<milliseconds> keys_query_timeout;
}

decltype(ircd::m::keys_query_timeout)
ircd::m::keys_query_timeout
{
	{ "name",     "ircd.keys.query.timeout" },
	{ "default",  20000L                    }
};

void
IRCD_MODULE_EXPORT
ircd::m::keys::query(const string_view &query_server,
                     const queries &queries,
                     const closure_bool &closure)
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

	request.wait(milliseconds(keys_query_timeout));
	const auto &code(request.get());
	const json::array response
	{
		request
	};

	for(const json::object &key : response) try
	{
		verify(m::keys{key});
		if(!closure(key))
			continue;

		cache::set(key);
	}
	catch(const std::exception &e)
	{
		log::derror
		{
			"Failed to verify keys for '%s' from '%s' :%s",
			key.get("server_name"),
			query_server,
			e.what()
		};
	}
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

//
// get
//

namespace ircd::m
{
	extern conf::item<milliseconds> keys_get_timeout;
}

decltype(ircd::m::keys_get_timeout)
ircd::m::keys_get_timeout
{
	{ "name",     "ircd.keys.get.timeout" },
	{ "default",  20000L                  }
};

void
IRCD_MODULE_EXPORT
ircd::m::keys::get(const string_view &server_name,
                   const closure &closure)
{
	return get(server_name, string_view{}, closure);
}

void
IRCD_MODULE_EXPORT
ircd::m::keys::get(const string_view &server_name,
                   const string_view &key_id,
                   const closure &closure)
try
{
	assert(!server_name.empty());

	if(cache::get(server_name, key_id, closure))
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

	request.wait(milliseconds(keys_get_timeout));
	const auto &status(request.get());
	const json::object response
	{
		request
	};

	const json::object &keys
	{
		response
	};

	verify(m::keys(keys));

	log::debug
	{
		m::log, "Verified keys from '%s'", server_name
	};

	cache::set(keys);
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

//
// m::keys::cache
//

size_t
IRCD_MODULE_EXPORT
ircd::m::keys::cache::set(const json::object &keys)
{
	const json::string &server_name
	{
		keys.at("server_name")
	};

	const m::node::room node_room
	{
		server_name
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
IRCD_MODULE_EXPORT
ircd::m::keys::cache::get(const string_view &server_name,
                          const string_view &key_id,
                          const keys::closure &closure)
{
	const m::node::room node_room
	{
		server_name
	};

	// Without a key_id we search for the most recent key; note this is not
	// the same as making a state_key="" query, as that would be an actual
	// ircd.key entry without an id (which shouldn't exist).
	const event::idx &event_idx
	{
		key_id?
			node_room.get(std::nothrow, "ircd.key", key_id):
			node_room.get(std::nothrow, "ircd.key")
	};

	if(!event_idx)
		return false;

	return m::get(std::nothrow, event_idx, "content", closure);
}

bool
IRCD_MODULE_EXPORT
ircd::m::keys::cache::for_each(const string_view &server_name,
                               const keys::closure_bool &closure)
{
	const m::node::room node_room
	{
		server_name
	};

	const m::room::state state
	{
		node_room
	};

	return state.for_each("ircd.key", [&closure]
	(const auto &type, const auto &key_id, const auto event_idx)
	{
		bool ret{true};
		m::get(std::nothrow, event_idx, "content", [&closure, &ret]
		(const json::object &content)
		{
			ret = closure(content);
		});

		return ret;
	});
}

///////////////////////////////////////////////////////////////////////////////
//
// (internal) ed25519 support sanity test
//

#ifdef RB_DEBUG
	#define TEST_ED25519 __attribute__((constructor))
#else
	#define TEST_ED25519
#endif

namespace ircd
{
	static void _test_ed25519() noexcept TEST_ED25519;
}

void
ircd::_test_ed25519()
noexcept
{
	char seed_buf[ed25519::SEED_SZ + 10];
	const auto seed
	{
		b64decode(seed_buf, "YJDBA9Xnr2sVqXD9Vj7XVUnmFZcZrlw8Md7kMW+3XA1")
	};

	ed25519::pk pk;
	ed25519::sk sk{&pk, seed};

	const auto SERVER_NAME {"domain"};
	const auto KEY_ID {"ed25519:1"};

	const auto test{[&]
	(const std::string &object) -> bool
	{
		const auto sig
		{
			sk.sign(const_buffer{object})
		};

		char sigb64_buf[128];
		const auto sigb64
		{
			b64encode_unpadded(sigb64_buf, sig)
		};

		ed25519::sig unsig; const auto unsigb64
		{
			b64decode(unsig, sigb64)
		};

		return pk.verify(const_buffer{object}, unsig);
	}};

	const bool tests[]
	{
		test(std::string{json::object
		{
			"{}"
		}}),

		test(json::strung(json::members
		{
			{ "one", 1L },
			{ "two", "Two" }
		})),
	};

	if(!std::all_of(begin(tests), end(tests), [](const bool &b) { return b; }))
		throw ircd::panic
		{
			"Seeded ed25519 test failed"
		};
}

///////////////////////////////////////////////////////////////////////////////
//
// m/self.h
//

//
// self::init
//

void
IRCD_MODULE_EXPORT
ircd::m::self::init::keys()
{
	tls_certificate();
	federation_ed25519();
}

namespace ircd::m::self
{
	extern conf::item<std::string> tls_key_dir;
}

decltype(ircd::m::self::tls_key_dir)
ircd::m::self::tls_key_dir
{
	{ "name",     "ircd.keys.tls_key_dir"  },
	{ "default",  fs::cwd()                }
};

void
IRCD_MODULE_EXPORT
ircd::m::self::init::tls_certificate()
{
	if(empty(self::origin))
		throw error
		{
			"The m::self::origin must be set to init my ed25519 key."
		};

	const std::string private_key_path_parts[]
	{
		std::string{tls_key_dir},
		self::origin + ".crt.key",
	};

	const std::string public_key_path_parts[]
	{
		std::string{tls_key_dir},
		self::origin + ".crt.key.pub",
	};

	const std::string dhparam_path_parts[]
	{
		std::string{tls_key_dir},
		self::origin + ".crt.dh",
	};

	const std::string certificate_path_parts[]
	{
		std::string{tls_key_dir},
		self::origin + ".crt",
	};

	const std::string private_key_file
	{
		fs::path_string(private_key_path_parts)
	};

	const std::string public_key_file
	{
		fs::path_string(public_key_path_parts)
	};

	const std::string cert_file
	{
		fs::path_string(certificate_path_parts)
	};

	if(!fs::exists(private_key_file) && !ircd::write_avoid)
	{
		log::warning
		{
			"Failed to find certificate private key @ `%s'; creating...",
			private_key_file
		};

		openssl::genrsa(private_key_file, public_key_file);
	}

	const json::object config{};
	if(!fs::exists(cert_file) && !ircd::write_avoid)
	{
		const json::object &certificate
		{
			config.get("certificate")
		};

		const json::object &self_
		{
			certificate.get(self::origin)
		};

		std::string subject
		{
			self_.get("subject")
		};

		if(empty(subject))
			subject = json::strung{json::members
			{
				{ "CN", self::origin }
			}};

		log::warning
		{
			"Failed to find SSL certificate @ `%s'; creating for '%s'...",
			cert_file,
			self::origin
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

//
// federation_ed25519
//

namespace ircd::m::self
{
	extern conf::item<std::string> ed25519_key_dir;
}

decltype(ircd::m::self::ed25519_key_dir)
ircd::m::self::ed25519_key_dir
{
	{ "name",     "ircd.keys.ed25519_key_dir"  },
	{ "default",  fs::cwd()                    }
};

void
IRCD_MODULE_EXPORT
ircd::m::self::init::federation_ed25519()
{
	if(empty(m::self::origin))
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
		ircd::string(fs::PATH_MAX_LEN, [&](const mutable_buffer &buf)
		{
			return fs::path(buf, path_parts);
		})
	};

	if(fs::exists(sk_file) || ircd::write_avoid)
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

//
// create_my_key
//

namespace ircd::m::self
{
	extern m::hookfn<m::vm::eval &> create_my_key_hook;
}

decltype(ircd::m::self::create_my_key_hook)
ircd::m::self::create_my_key_hook
{
	{
		{ "_site",     "vm.effect"           },
		{ "room_id",   m::my_node.room_id()  },
		{ "type",      "m.room.create"       },
	},
	[](const m::event &, m::vm::eval &)
	{
		create_my_key();
	}
};

void
IRCD_MODULE_EXPORT
ircd::m::self::create_my_key()
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

	//TODO: conf
	json::get<"valid_until_ts"_>(my_key) =
		ircd::time<milliseconds>() + milliseconds(1000UL * 60 * 60 * 24 * 180).count();

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
	keys::cache::set(json::strung{my_key});
}
