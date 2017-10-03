/*
 * charybdis: 21st Century IRC++d
 *
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once
#define HAVE_IRCD_M_KEYS_H
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsubobject-linkage"

namespace ircd::m
{
	struct key;
}

namespace ircd::my
{
	extern ed25519::pk public_key;
	extern ed25519::sk secret_key;
	extern std::string public_key_b64;
}

namespace ircd::m::name
{
	constexpr const char *const server_name {"server_name"};
	constexpr const char *const verify_keys {"verify_keys"};
	constexpr const char *const old_verify_keys {"old_verify_keys"};
//	constexpr const char *const signatures {"signatures"};
	constexpr const char *const tls_fingerprints {"tls_fingerprints"};
	constexpr const char *const valid_until_ts {"valid_until_ts"};
}

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
struct ircd::m::key
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
	static room keys;

	using super_type::tuple;
	using super_type::operator=;
};

namespace ircd::m::keys
{
	using closure = std::function<void (const key &)>;

	bool get(const string_view &server_name, const closure &);
	void set(const key &);
}

#pragma GCC diagnostic pop
