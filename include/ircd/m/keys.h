// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_KEYS_H

namespace ircd::m
{
	struct keys;
}

namespace ircd::m::self
{
	extern ed25519::sk secret_key;
	extern ed25519::pk public_key;
	extern std::string public_key_b64;
	extern std::string public_key_id;

	extern std::string tls_cert_der;
	extern std::string tls_cert_der_sha256_b64;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"
/// Contains the public keys and proof of identity for a remote server.
///
/// A user who wishes to verify a signature from a remote server must have
/// the name of the server (origin) and the key_id. Calling the appropriate
/// static function of this class will attempt to fetch the key from the db
/// or make network requests, with valid response being saved to the db. Keys
/// are thus managed internally so the user doesn't supply a buffer or ever
/// construct this object; instead this object backed by internal db data is
/// presented in the supplied synchronous closure.
///
/// 2.2.1.1 Publishing Keys
///
/// Key                 Type, Description
/// server_name         String, DNS name of the homeserver.
/// verify_keys         Object, Public keys of the homeserver for verifying digital signatures.
/// old_verify_keys     Object, The public keys that the server used to use and when it stopped using them.
/// signatures          Object, Digital signatures for this object signed using the verify_keys.
/// tls_fingerprints    Array of Objects, Hashes of X.509 TLS certificates used by this this server encoded as Unpadded Base64.
/// valid_until_ts      Integer, POSIX timestamp when the list of valid keys should be refreshed.
///
struct ircd::m::keys
:json::tuple
<
	json::property<name::old_verify_keys, json::object>,
	json::property<name::server_name, string_view>,
	json::property<name::signatures, json::object>,
	json::property<name::tls_fingerprints, json::array>,
	json::property<name::valid_until_ts, time_t>,
	json::property<name::verify_keys, json::object>
>
{
	struct init;

  public:
	using closure = std::function<void (const keys &)>;
	using closure_bool = std::function<bool (const keys &)>;
	using key_closure = std::function<void (const string_view &)>;  // remember to unquote()!!!
	using ed25519_closure = std::function<void (const ed25519::pk &)>;
	using queries = vector_view<const std::pair<string_view, string_view>>; // server, key_id

	static void get(const string_view &server_name, const closure &);
	static void get(const string_view &server_name, const string_view &key_id, const closure &);
	static void get(const string_view &server_name, const string_view &key_id, const key_closure &);
	static void get(const string_view &server_name, const string_view &key_id, const ed25519_closure &);
	static bool query(const string_view &query_server, const queries &, const closure_bool &);

	using super_type::tuple;
	using super_type::operator=;
};
#pragma GCC diagnostic pop

struct ircd::m::keys::init
{
	json::object config;

	void certificate();
	void signing();

  public:
	init(const json::object &config);
	~init() noexcept;
};
