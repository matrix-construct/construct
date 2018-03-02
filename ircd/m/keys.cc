// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/m/m.h>

const ircd::m::room::id::buf
keys_room_id
{
	"keys", ircd::my_host()
};

/// The keys room is where the public key data for each server is stored as
/// state indexed by the server name.
ircd::m::room
ircd::m::keys::room
{
	keys_room_id
};

ircd::ed25519::sk
ircd::m::self::secret_key
{};

ircd::ed25519::pk
ircd::m::self::public_key
{};

std::string
ircd::m::self::public_key_b64
{};

std::string
ircd::m::self::public_key_id
{};

std::string
ircd::m::self::tls_cert_der
{};

std::string
ircd::m::self::tls_cert_der_sha256_b64
{};

//
// init
//

ircd::m::keys::init::init(const json::object &config)
:config{config}
{
	certificate();
	signing();
}

ircd::m::keys::init::~init()
noexcept
{
}

void
ircd::m::keys::init::certificate()
{
	const std::string cert_file
	{
		unquote(conf.at("tls_certificate_path"))
	};

	if(!fs::exists(cert_file))
		throw fs::error("Failed to find SSL certificate @ `%s'", cert_file);

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

	self::tls_cert_der_sha256_b64 =
	{
		b64encode_unpadded(hash)
	};

	static char print_buf[8_KiB];
	log.info("Certificate `%s' (PEM: %zu bytes; DER: %zu bytes) sha256b64: %s :%s",
	         cert_file,
	         cert_pem.size(),
	         ircd::size(cert_der),
	         self::tls_cert_der_sha256_b64,
	         openssl::print_subject(print_buf, cert_pem));
}

void
ircd::m::keys::init::signing()
{
	const std::string sk_file
	{
		unquote(config.get("signing_key_path", "construct.sk"))
	};

	if(fs::exists(sk_file))
		log.info("Using ed25519 secret key @ `%s'", sk_file);
	else
		log.notice("Creating new ed25519 secret key @ `%s'", sk_file);

	self::secret_key = ed25519::sk
	{
		sk_file, &self::public_key
	};

	self::public_key_b64 = b64encode_unpadded(self::public_key);
	const fixed_buffer<const_buffer, sha256::digest_size> hash
	{
		sha256{self::public_key}
	};

	const auto public_key_hash_b58
	{
		b58encode(hash)
	};

	static const auto trunc_size{8};
	self::public_key_id = fmt::snstringf
	{
		BUFSIZE, "ed25519:%s", trunc(public_key_hash_b58, trunc_size)
	};

	log.info("Current key is '%s' and the public key is: %s",
	         self::public_key_id,
	         self::public_key_b64);
}

void
ircd::m::keys::init::bootstrap()
{
	create(keys::room, me.user_id);
	send(keys::room, me.user_id, "m.room.name", "",
	{
		{ "name", "Key Room" }
	});

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

	keys my_key;
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
	keys::set(my_key);
}

//
// keys
//

void
ircd::m::keys::get(const string_view &server_name,
                   const string_view &key_id,
                   const key_closure &closure)
try
{
	get(server_name, [&key_id, &closure](const keys &keys)
	{
		const json::object vks
		{
			at<"verify_keys"_>(keys)
		};

		const json::object vkk
		{
			vks.at(key_id)
		};

		// The key is not unquote() because some types of keys may be
		// more complex than just a string one day; think: RLWE.
		const string_view &key
		{
			vkk.at("key")
		};

		closure(key);
	});
}
catch(const json::not_found &e)
{
	throw m::NOT_FOUND
	{
		"Failed to find key '%s' for '%s': %s",
		key_id,
		server_name,
		e.what()
	};
}

void
ircd::m::keys::get(const string_view &server_name,
                   const keys_closure &closure)
{
	assert(!server_name.empty());

	if(get_local(server_name, closure))
		return;

	if(server_name == my_host())
		throw m::NOT_FOUND
		{
			"keys for '%s' (that's myself) not found", server_name
		};

	log.debug("Keys for %s not cached; querying network...",
	          server_name);

	char url[1024]; const auto url_len
	{
		fmt::snprintf(url, sizeof(url), "_matrix/key/v2/server/")
	};

	//TODO: XXX
	const unique_buffer<mutable_buffer> buffer
	{
		8192
	};

	ircd::parse::buffer pb{mutable_buffer{buffer}};
	m::request request{"GET", url, {}, {}};
	m::session session{server_name};
	const json::object response
	{
		session(pb, request)
	};

	const m::keys &keys
	{
		response
	};

	if(!verify(keys))
		throw m::error
		{
			http::UNAUTHORIZED, "M_INVALID_SIGNATURE",
			"Failed to verify keys for '%s'", server_name
		};

	log.debug("Verified keys from '%s'",
	          server_name);

	set(keys);
	closure(keys);
}

void
ircd::m::keys::get(const string_view &server_name,
                   const string_view &key_id,
                   const string_view &query_server,
                   const keys_closure &closure)
try
{
	assert(!server_name.empty());
	assert(!query_server.empty());

	char key_id_buf[1024];
	char server_name_buf[1024];
	char url[1024]; const auto url_len
	{
		fmt::snprintf(url, sizeof(url), "_matrix/key/v2/query/%s/%s/",
		              url::encode(server_name, server_name_buf),
		              url::encode(key_id, key_id_buf))
	};

	//TODO: XXX
	const unique_buffer<mutable_buffer> buffer
	{
		8192
	};

	// Make request and receive response synchronously.
	// This ircd::ctx will block here fetching.
	ircd::parse::buffer pb{mutable_buffer{buffer}};
	m::request request{"GET", url, {}, {}};
	m::session session{server_name};
	const json::object response
	{
		session(pb, request)
	};

	const json::array &keys
	{
		response.at("server_keys")
	};

	log::debug("Fetched %zu candidate keys seeking '%s' for '%s' from '%s' (%s)",
	           keys.count(),
	           empty(key_id)? "*" : key_id,
	           server_name,
	           query_server,
	           string(net::remote(session.server)));

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

		log.debug("Verified keys for '%s' from '%s'",
		          _server_name,
		          query_server);

		set(keys);
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

bool
ircd::m::keys::get_local(const string_view &server_name,
                         const keys_closure &closure)
{
	static const string_view type{"ircd.key"};
	return m::keys::room.get(std::nothrow, type, server_name, [&closure]
	(const m::event &event)
	{
		closure(json::get<"content"_>(event));
	});
}

void
ircd::m::keys::set(const keys &keys)
{
	const auto &server_name
	{
		unquote(at<"server_name"_>(keys))
	};

	const auto &state_key
	{
		server_name
	};

	const m::user::id::buf sender
	{
		"ircd", server_name
	};

	const json::strung derp
	{
		keys
	};

	static const string_view type{"ircd.key"};
	send(keys::room, sender, type, state_key, json::object{derp});
}

/// Verify this key data (with itself).
bool
ircd::m::keys::verify(const keys &keys)
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
	log.error("key verification for '%s' failed: %s",
	          json::get<"server_name"_>(keys, "<no server name>"_sv),
	          e.what());

	return false;
}
