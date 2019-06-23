// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_OPENSSL_ERR_H
#include <RB_INC_OPENSSL_ASN1_H
#include <RB_INC_OPENSSL_SHA_H
#include <RB_INC_OPENSSL_HMAC_H
#include <RB_INC_OPENSSL_SSL_H
#include <RB_INC_OPENSSL_EC_H
#include <RB_INC_OPENSSL_RSA_H
#include <RB_INC_OPENSSL_X509_H
#include <RB_INC_OPENSSL_EVP_H
#include <RB_INC_OPENSSL_RIPEMD_H
#include <RB_INC_OPENSSL_DH_H
#include <RB_INC_OPENSSL_TLS1_H

// Metaconditions for which OpenSSL API to use. This produces a single #define
// to simplify further #ifdef's throught this definition file.
#if defined(LIBRESSL_VERSION_NUMBER) || OPENSSL_VERSION_NUMBER < 0x10100000L
	#define IRCD_OPENSSL_API_1_0_X
#else
	#define IRCD_OPENSSL_API_1_1_X
#endif

#if defined(LIBRESSL_VERSION_NUMBER)
static time_t ASN1_TIME_seconds(const ASN1_TIME *);
static int ASN1_TIME_diff(int *, int *, const ASN1_TIME *, const ASN1_TIME *);
#endif

namespace ircd::openssl
{
	template<class exception = openssl::error>
	[[noreturn]] static void throw_error(const ulong &);

	template<class exception = openssl::error>
	[[noreturn]] static void throw_error();

	template<class exception = openssl::error,
	         int ERR_CODE = 0,
	         class function,
	         class... args>
	static int call(function&& f, args&&... a);

	static int genprime_cb(const int, const int, BN_GENCB *const) noexcept;
}

///////////////////////////////////////////////////////////////////////////////
//
// openssl.h
//

decltype(ircd::openssl::version_api)
ircd::openssl::version_api
{
	"OpenSSL", info::versions::API, OPENSSL_VERSION_NUMBER, {0}, OPENSSL_VERSION_TEXT
};

decltype(ircd::openssl::version_abi)
ircd::openssl::version_abi
{
	"OpenSSL", info::versions::ABI, long(::SSLeay()), {0}, ::SSLeay_version(SSLEAY_VERSION)
};

#ifdef LIBRESSL_VERSION_NUMBER
decltype(ircd::openssl::libressl_version_api)
ircd::openssl::libressl_version_api
{
	"LibreSSL", info::versions::API, LIBRESSL_VERSION_NUMBER, {0}, LIBRESSL_VERSION_TEXT
};
#endif LIBRESSL_VERSION_NUMBER

//
// SNI
//

void
ircd::openssl::server_name(SSL &ssl,
                           const string_view &name)
{
	thread_local char buf[256];
	strlcpy(buf, name);
	call(::SSL_ctrl, &ssl, SSL_CTRL_SET_TLSEXT_HOSTNAME, TLSEXT_NAMETYPE_host_name, buf);
}

ircd::string_view
ircd::openssl::server_name(const SSL &ssl)
{
	const int type(::SSL_get_servername_type(&ssl));
	return ::SSL_get_servername(&ssl, type);
}

//
// Cipher suite
//

void
ircd::openssl::set_curves(SSL &ssl,
                          std::string list)
{
	auto data(const_cast<char *>(list.data()));
	call(::SSL_ctrl, &ssl, SSL_CTRL_SET_CURVES_LIST, 0, data);
}

void
ircd::openssl::set_curves(SSL_CTX &ssl,
                          std::string list)
{
	auto data(const_cast<char *>(list.data()));
	call(::SSL_CTX_ctrl, &ssl, SSL_CTRL_SET_CURVES_LIST, 0, data);
}

void
ircd::openssl::set_tmp_ecdh(SSL_CTX &ssl,
                            EC_KEY &key)
{
	auto data(reinterpret_cast<char *>(&key));
	call(::SSL_CTX_ctrl, &ssl, SSL_CTRL_SET_TMP_ECDH, 0, data);
}

void
ircd::openssl::set_ecdh_auto(SSL &ssl,
                             const bool &on)
{
	#ifdef IRCD_OPENSSL_API_1_0_X
	long _on(on);
	call(::SSL_ctrl, &ssl, SSL_CTRL_SET_ECDH_AUTO, _on, nullptr);
	#endif
}

void
ircd::openssl::set_ecdh_auto(SSL_CTX &ssl,
                             const bool &on)
{
	#ifdef IRCD_OPENSSL_API_1_0_X
	long _on(on);
	call(::SSL_CTX_ctrl, &ssl, SSL_CTRL_SET_ECDH_AUTO, _on, nullptr);
	#endif
}

void
ircd::openssl::set_cipher_list(SSL_CTX &ssl,
                               const std::string &list)
{
	call(::SSL_CTX_set_cipher_list, &ssl, list.c_str());
}

void
ircd::openssl::set_cipher_list(SSL &ssl,
                               const std::string &list)
{
	call(::SSL_set_cipher_list, &ssl, list.c_str());
}

std::string
ircd::openssl::cipher_list(const SSL_CTX &ctx,
                           const int &priority)
{
	const custom_ptr<SSL> ssl
	{
		SSL_new(const_cast<SSL_CTX *>(&ctx)), SSL_free
	};

	std::stringstream ret;
	for(int i(priority); priority? i <= priority : true; ++i)
	{
		const auto cipher(cipher_list(*ssl, i));
		if(!empty(cipher))
			ret << cipher << ':';
		else
			break;
	}

	return ret.str();
}

ircd::string_view
ircd::openssl::cipher_list(const SSL &ssl,
                           const int &priority)
{
	return SSL_get_cipher_list(&ssl, priority);
}

ircd::string_view
ircd::openssl::shared_ciphers(const mutable_buffer &buf,
                              const SSL &ssl)
{
	return SSL_get_shared_ciphers(&ssl, data(buf), size(buf));
}

const SSL_CIPHER *
ircd::openssl::current_cipher(const SSL &ssl)
{
	return SSL_get_current_cipher(&ssl);
}

ircd::string_view
ircd::openssl::name(const SSL_CIPHER &cipher)
{
	return SSL_CIPHER_get_name(&cipher);
}

//
// X509
//

namespace ircd::openssl
{
	time_t get_time(const ASN1_TIME &);
	using x509_name_entry_closure = std::function<bool (const string_view &, const string_view &)>;
	bool for_each(const X509_NAME &name, const x509_name_entry_closure &);
	void append(X509_NAME &name, const string_view &key, const string_view &val);
	void append(X509_NAME &name, const json::object &entries);
	void append_entries(X509 &cert, const json::object &opts);
}

X509 &
ircd::openssl::current_cert(X509_STORE_CTX &cx)
{
	auto *const ret
	{
		X509_STORE_CTX_get_current_cert(&cx)
	};

	if(unlikely(!ret))
		throw error
		{
			"No current certificate"
		};

	return *ret;
}

const X509 &
ircd::openssl::current_cert(const X509_STORE_CTX &cx)
{
	auto &mcx{const_cast<X509_STORE_CTX &>(cx)};
	const auto *const ret
	{
		X509_STORE_CTX_get_current_cert(&mcx)
	};

	if(unlikely(!ret))
		throw error
		{
			"No current certificate"
		};

	return *ret;
}

uint
ircd::openssl::get_error_depth(const X509_STORE_CTX &cx)
{
	auto &mcx{const_cast<X509_STORE_CTX &>(cx)};
	const int ret
	{
		X509_STORE_CTX_get_error_depth(&mcx)
	};

	assert(ret >= 0);
	return ret;
}

const char *
ircd::openssl::get_error_string(const X509_STORE_CTX &cx)
{
	return cert_error_string(get_error(cx));
}

const char *
ircd::openssl::cert_error_string(const long &n)
{
	return X509_verify_cert_error_string(n);
}

int
ircd::openssl::get_error(const X509_STORE_CTX &cx)
{
	auto &mcx{const_cast<X509_STORE_CTX &>(cx)};
	return X509_STORE_CTX_get_error(&mcx);
}

X509 &
ircd::openssl::peer_cert(SSL &ssl)
{
	auto *const ret
	{
		SSL_get_peer_certificate(&ssl)
	};

	assert(ret);
	if(unlikely(!ret))
		throw error
		{
			"No X509 certificate for peer"
		};

	return *ret;
}

const X509 &
ircd::openssl::peer_cert(const SSL &ssl)
{
	const auto *const ret
	{
		SSL_get_peer_certificate(&ssl)
	};

	assert(ret);
	if(unlikely(!ret))
		throw error
		{
			"No X509 certificate for peer"
		};

	return *ret;
}

namespace ircd::openssl
{
	static void genx509_readkeys(EVP_PKEY &, const json::object &);
}

ircd::string_view
ircd::openssl::genX509_rsa(const mutable_buffer &out,
                           const json::object &opts)
{
	const custom_ptr<RSA> priv
	{
		RSA_new(), RSA_free
	};

	const custom_ptr<EVP_PKEY> pk
	{
		EVP_PKEY_new(), EVP_PKEY_free
	};

	set(*pk, *priv);
	genx509_readkeys(*pk, opts);
	check(*EVP_PKEY_get1_RSA(pk.get()));
	return genX509(out, *pk, opts);
}

ircd::string_view
ircd::openssl::genX509_ec(const mutable_buffer &out,
                          const json::object &opts)
{
	const custom_ptr<EC_KEY> priv
	{
		EC_KEY_new(), EC_KEY_free
	};

	const custom_ptr<EVP_PKEY> pk
	{
		EVP_PKEY_new(), EVP_PKEY_free
	};

	set(*pk, *priv);
	genx509_readkeys(*pk, opts);
	check(*EVP_PKEY_get1_EC_KEY(pk.get()));
	return genX509(out, *pk, opts);
}

void
ircd::openssl::genx509_readkeys(EVP_PKEY &pk,
                                const json::object &opts)
{
	const std::string private_key_path
	{
		unquote(opts.at("private_key_pem_path"))
	};

	const std::string public_key_path
	{
		unquote(opts.get("public_key_pem_path", private_key_path + ".pub"))
	};

	bio::read_file(private_key_path, [&pk](const string_view &pem)
	{
		read_pem_priv(pk, pem);
	});

	bio::read_file(public_key_path, [&pk](const string_view &pem)
	{
		read_pem_pub(pk, pem);
	});
}

ircd::string_view
ircd::openssl::genX509(const mutable_buffer &out,
                       EVP_PKEY &pk,
                       const json::object &opts)
{
	const custom_ptr<X509> x509
	{
		X509_new(), X509_free
	};

	call(::X509_set_pubkey, x509.get(), &pk);
	append_entries(*x509, opts);

	call(::X509_sign, x509.get(), &pk, EVP_sha256());

	return write_pem(out, *x509);
}

std::string
ircd::openssl::stringify(const X509 &cert_)
{
	auto &cert{const_cast<X509 &>(cert_)};

	// issuer
	std::vector<json::member> issuer_json;
	X509_NAME *const issuer{X509_get_issuer_name(&cert)};
	for_each(*issuer, [&](const string_view &key, const string_view &val)
	{
		const json::member member{key, val};
		issuer_json.emplace_back(member);
		return true;
	});

	// subject
	std::vector<json::member> subject_json;
	X509_NAME *const subject{X509_get_subject_name(&cert)};
	for_each(*subject, [&](const string_view &key, const string_view &val)
	{
		const json::member member{key, val};
		subject_json.emplace_back(member);
		return true;
	});

	return json::strung{json::members
	{
		{ "issuer",     { issuer_json.data(), issuer_json.size()    } },
		{ "subject",    { subject_json.data(), subject_json.size()  } },
		{ "notBefore",  not_before(cert)                              },
		{ "notAfter",   not_after(cert)                               },
	}};
}

void
ircd::openssl::append_entries(X509 &cert,
                              const json::object &opts)
{
	// version
	call(::X509_set_version, &cert, opts.get<long>("version", 2));

	// notBefore
	{
		const long value
		{
			opts.get<long>("notBefore", 0)
		};

		ASN1_TIME *const notBefore{X509_get_notBefore(&cert)};
		assert(notBefore != nullptr);
		X509_gmtime_adj(notBefore, value);
	}

	// notAfter
	{
		const long value
		{
			opts.get<long>("notAfter", 0)?:
			60 * 60 * 24 * opts.get<long>("days", 60L)
		};

		ASN1_TIME *const notAfter{X509_get_notAfter(&cert)};
		assert(notAfter != nullptr);
		X509_gmtime_adj(notAfter, value);
	}

	// subject
	if(opts.has("subject"))
	{
		const json::object subject_opts
		{
			opts["subject"]
		};

		X509_NAME *const subject
		{
			X509_get_subject_name(&cert)
		};

		assert(subject != nullptr);
		append(*subject, subject_opts);
	}

	// issuer
	if(opts.has("issuer"))
	{
		const json::object issuer_opts
		{
			opts["issuer"]
		};

		X509_NAME *const issuer
		{
			X509_get_issuer_name(&cert)
		};

		assert(issuer != nullptr);
		append(*issuer, issuer_opts);
	}
	else if(opts.has("subject")) // self-signed; issuer is subject
	{
		X509_NAME *const subject
		{
			X509_get_subject_name(&cert)
		};

		assert(subject != nullptr);
		call(::X509_set_issuer_name, &cert, subject);
	}
}

void
ircd::openssl::append(X509_NAME &name,
                      const json::object &entries)
{
	for(const auto &member : entries)
		append(name, unquote(member.first), unquote(member.second));
}

void
ircd::openssl::append(X509_NAME &name,
                      const string_view &key,
                      const string_view &val)
try
{
	call(::X509_NAME_add_entry_by_txt,
	     &name,
	     std::string{key}.c_str(),     // key (has to be null terminated)
	     MBSTRING_ASC,                 // type
	     (const uint8_t *)val.data(),  // data
	     val.size(),                   // len
	     -1,                           // loc (-1 = append)
	     0);                           // set (0 = new RDN created)
}
catch(const error &e)
{
	throw error
	{
		"Failed to append X509 NAME entry '%s' (%zu bytes): %s",
		key,
		val.size(),
		e.what()
	};
}

bool
ircd::openssl::for_each(const X509_NAME &name_,
                        const x509_name_entry_closure &closure)
{
	const auto name(const_cast<X509_NAME *>(&name_));
	const auto cnt(X509_NAME_entry_count(name));
	for(auto i(0); i < cnt; ++i)
	{
		const auto entry(X509_NAME_get_entry(name, i));
		const auto obj(X509_NAME_ENTRY_get_object(entry));

		thread_local char keybuf[128];
		const ssize_t keylen(OBJ_obj2txt(keybuf, sizeof(keybuf), obj, 0));
		if(unlikely(keylen < 0))
			continue;

		thread_local char valbuf[1024];
		const ssize_t vallen(X509_NAME_get_text_by_OBJ(name, obj, valbuf, sizeof(valbuf)));
		if(unlikely(vallen < 0))
			continue;

		const string_view key{keybuf, size_t(keylen)};
		const string_view val{valbuf, size_t(vallen)};
		if(!closure(key, val))
			return false;
	}

	return true;
}

time_t
ircd::openssl::not_before(const X509 &cert_)
{
	auto &cert{const_cast<X509 &>(cert_)};
	ASN1_TIME *const notBefore{X509_get_notBefore(&cert)};
	return get_time(*notBefore);
}

time_t
ircd::openssl::not_after(const X509 &cert_)
{
	auto &cert{const_cast<X509 &>(cert_)};
	ASN1_TIME *const notAfter{X509_get_notAfter(&cert)};
	return get_time(*notAfter);
}

ircd::string_view
ircd::openssl::subject_common_name(const mutable_buffer &out,
                                   const X509 &cert)
{
	X509_NAME *const subject
	{
		X509_get_subject_name(const_cast<X509 *>(&cert))
	};

	if(!subject)
		return {};

	const auto len
	{
		X509_NAME_get_text_by_NID(subject, NID_commonName, data(out), size(out))
	};

	// NID_commonName does not exist in subject.
	if(len < 0)
		return {};

	// Terminating NULL is written to buffer but is not counted in len.
	assert(size_t(len) < size(out));
	return { data(out), size_t(len) };
}

ircd::string_view
ircd::openssl::print_subject(const mutable_buffer &buf,
                             const string_view &pem,
                             ulong flags)
{
	const custom_ptr<X509> x509
	{
		X509_new(), X509_free
	};

	return print_subject(buf, read_pem(*x509, pem), flags);
}

ircd::string_view
ircd::openssl::print_subject(const mutable_buffer &buf,
                             const X509 &cert,
                             ulong flags)
{
	if(flags == ulong(-1))
		flags = XN_FLAG_ONELINE;
	else
		flags = 0;

	const X509_NAME *const subject
	{
		X509_get_subject_name(const_cast<X509 *>(&cert))
	};

	return bio::write(buf, [&subject, &flags]
	(BIO *const &bio)
	{
		X509_NAME_print_ex(bio, const_cast<X509_NAME *>(subject), 0, flags);
	});
}

ircd::string_view
ircd::openssl::printX509(const mutable_buffer &buf,
                         const string_view &pem,
                         ulong flags)
{
	const custom_ptr<X509> x509
	{
		X509_new(), X509_free
	};

	return print(buf, read_pem(*x509, pem), flags);
}

ircd::string_view
ircd::openssl::print(const mutable_buffer &buf,
                     const X509 &cert,
                     ulong flags)
{
	if(flags == ulong(-1))
		flags = XN_FLAG_ONELINE;
	else
		flags = 0;

	return bio::write(buf, [&cert, &flags]
	(BIO *const &bio)
	{
		X509_print_ex(bio, const_cast<X509 *>(&cert), 0, flags);
	});
}

ircd::const_buffer
ircd::openssl::cert2d(const mutable_buffer &out,
                      const string_view &pem)
{
	const custom_ptr<X509> x509
	{
		X509_new(), X509_free
	};

	return i2d(out, read_pem(*x509, pem));
}

X509 &
ircd::openssl::read_pem(X509 &out_,
                        const string_view &pem)
{
	X509 *ret{nullptr}, *out{&out_};
	bio::read(pem, [&ret, &out]
	(BIO *const &bio)
	{
		ret = PEM_read_bio_X509(bio, &out, nullptr, nullptr);
	});

	if(unlikely(ret != out))
		throw error
		{
			"Failed to read X509 PEM @ %p (len: %zu)", pem.data(), pem.length()
		};

	return *ret;
}

ircd::string_view
ircd::openssl::write_pem(const mutable_buffer &out,
                         const X509 &cert)
{
	return bio::write(out, [&cert]
	(BIO *const &bio)
	{
		call(::PEM_write_bio_X509, bio, const_cast<X509 *>(&cert));
	});
}

ircd::const_buffer
ircd::openssl::i2d(const mutable_buffer &buf,
                   const X509 &_cert)
{
	auto &cert
	{
		const_cast<X509 &>(_cert)
	};

	const int len
	{
		i2d_X509(&cert, nullptr)
	};

	if(unlikely(len < 0))
		throw_error();

	if(unlikely(size(buf) < size_t(len)))
		throw error
		{
			"DER requires a %zu byte buffer, you supplied %zu bytes", len, size(buf)
		};

	auto *out(reinterpret_cast<uint8_t *>(data(buf)));
	const const_buffer ret
	{
		data(buf), size_t(i2d_X509(&cert, &out))
	};

	if(unlikely(size(ret) != size_t(len)))
		throw error();

	assert(out - reinterpret_cast<uint8_t *>(data(buf)) == len);
	return ret;
}

time_t
ircd::openssl::get_time(const ASN1_TIME &t)
{
	int pday, psec;
	ASN1_TIME_diff(&pday, &psec, nullptr, &t);
	const time_t sec
	{
		pday * 60L * 60L * 24L + psec
	};

	return ircd::time() + sec;
}

//
// DH
//

decltype(ircd::openssl::rfc3526_dh_params_pem)
ircd::openssl::rfc3526_dh_params_pem
{R"(
2048-bit DH parameters taken from rfc3526
-----BEGIN DH PARAMETERS-----
MIIBCAKCAQEA///////////JD9qiIWjCNMTGYouA3BzRKQJOCIpnzHQCC76mOxOb
IlFKCHmONATd75UZs806QxswKwpt8l8UN0/hNW1tUcJF5IW1dmJefsb0TELppjft
awv/XLb0Brft7jhr+1qJn6WunyQRfEsf5kkoZlHs5Fs9wgB8uKFjvwWY2kg2HFXT
mmkWP6j9JM9fg2VdI9yjrZYcYvNWIIVSu57VKQdwlpZtZww1Tkq8mATxdGwIyhgh
fDKQXkYuNs474553LBgOhgObJ4Oi7Aeij7XFXfBvTFLJ3ivL9pVYFxg5lUl86pVq
5RXSJhiY+gUQFXKOWoqsqmj//////////wIBAg==
-----END DH PARAMETERS-----
)"};

decltype(ircd::openssl::DH_DEFAULT_BITS)
ircd::openssl::DH_DEFAULT_BITS
{
	2048
};

decltype(ircd::openssl::DH_DEFAULT_GEN)
ircd::openssl::DH_DEFAULT_GEN
{
	5
};

void
ircd::openssl::gendh(const string_view &dhfile,
                     const uint &bits,
                     const uint &gen)
{
	bio::write_file(dhfile, [&bits, &gen]
	(const mutable_buffer &buf)
	{
		return gendh(buf, bits, gen);
	});
}

ircd::string_view
ircd::openssl::gendh(const mutable_buffer &buf,
                     const uint &bits,
                     const uint &gen)
{
	const custom_ptr<DH> dh
	{
		DH_new(), DH_free
	};

	gendh(*dh, bits, gen);
	return bio::write(buf, [&dh]
	(BIO *const &bio)
	{
		call(::DHparams_print, bio, dh.get());
	});
}

DH &
ircd::openssl::gendh(DH &dh,
                     const uint &bits,
                     const uint &gen)
{
	#ifdef IRCD_OPENSSL_API_1_1_X
	const custom_ptr<BN_GENCB> gencb
	{
		BN_GENCB_new(), BN_GENCB_free
	};
	#else
	const std::unique_ptr<BN_GENCB> gencb
	{
		std::make_unique<BN_GENCB>()
	};
	memset(gencb.get(), 0x0, sizeof(BN_GENCB));
	#endif

	void *const arg{nullptr}; // privdata passed to cb
	BN_GENCB_set(gencb.get(), &ircd::openssl::genprime_cb, arg);

	call<error, 0>(::DH_generate_parameters_ex, &dh, bits, gen, gencb.get());
	return dh;
}

//
// EC
//

namespace ircd::openssl
{
	void ec_init();
	void ec_fini() noexcept;
}

const EC_GROUP *
ircd::openssl::secp256k1
{};

void
ircd::openssl::ec_init()
{
	EC_GROUP *_secp256k1;
	if(!(_secp256k1 = EC_GROUP_new_by_curve_name(OBJ_sn2nid("secp256k1"))))
		throw error{"Failed to initialize EC_GROUP secp256k1"};

	EC_GROUP_set_asn1_flag(_secp256k1, OPENSSL_EC_NAMED_CURVE);
	EC_GROUP_set_point_conversion_form(_secp256k1, POINT_CONVERSION_COMPRESSED);
	secp256k1 = _secp256k1;
}

void
ircd::openssl::ec_fini()
noexcept
{
	EC_GROUP_free(const_cast<EC_GROUP *>(secp256k1));
}

void
ircd::openssl::genec(const string_view &skfile,
                     const string_view &pkfile,
                     const EC_GROUP *const &group)
{
	const custom_ptr<EC_KEY> key
	{
		EC_KEY_new(), EC_KEY_free
	};

	const custom_ptr<EVP_PKEY> pk
	{
		EVP_PKEY_new(), EVP_PKEY_free
	};

	const auto write_priv{[&pk](const mutable_buffer &out)
	{
		return write_pem_priv(out, *pk);
	}};

	const auto write_pub{[&pk](const mutable_buffer &out)
	{
		return write_pem_pub(out, *pk);
	}};

	assert(group);
	assert(EC_GROUP_get_asn1_flag(group) & OPENSSL_EC_NAMED_CURVE);
	call(::EC_KEY_set_group, key.get(), group);
	call(::EC_KEY_generate_key, key.get());
	assert(EC_KEY_get0_public_key(key.get()));
	set(*pk, *key);
	bio::write_file(skfile, write_priv);
	bio::write_file(pkfile, write_pub);
}

ircd::string_view
ircd::openssl::print(const mutable_buffer &buf,
                     const EC_KEY &key,
                     const off_t &offset)
{
	return bio::write(buf, [&key, &offset]
	(BIO *const &bio)
	{
		call(::EC_KEY_print, bio, &key, offset);
	});
}

void
ircd::openssl::check(const EC_KEY &key)
{
	if(!check(key, std::nothrow))
		throw error{"Invalid Elliptic Curve Key"};
}

bool
ircd::openssl::check(const EC_KEY &key,
                     const std::nothrow_t)
{
	return EC_KEY_check_key(&key) == 1;
}

//
// RSA
//

void
ircd::openssl::genrsa(const string_view &skfile,
                      const string_view &pkfile,
                      const json::object &opts)
{
	const auto bits
	{
		opts.get<uint>("bits", 2048)
	};

	const auto e
	{
		opts.get<uint>("e", 65537)
	};

	const custom_ptr<RSA> rsa
	{
		RSA_new(), RSA_free
	};

	const custom_ptr<EVP_PKEY> pk
	{
		EVP_PKEY_new(), EVP_PKEY_free
	};

	genrsa(*rsa, bits, e);
	check(*rsa);
	set(*pk, *rsa);

	bio::write_file(skfile, [&pk]
	(const mutable_buffer &out)
	{
		return write_pem_priv(out, *pk);
	});

	bio::write_file(pkfile, [&pk]
	(const mutable_buffer &out)
	{
		return write_pem_pub(out, *pk);
	});
}

RSA &
ircd::openssl::genrsa(RSA &out,
                      const uint &bits,
                      const uint &exp)
{
	#ifdef IRCD_OPENSSL_API_1_1_X
	const custom_ptr<BN_GENCB> gencb
	{
		BN_GENCB_new(), BN_GENCB_free
	};
	#else
	const std::unique_ptr<BN_GENCB> gencb
	{
		std::make_unique<BN_GENCB>()
	};
	memset(gencb.get(), 0x0, sizeof(BN_GENCB));
	#endif

	void *const arg{nullptr}; // privdata passed to cb
	BN_GENCB_set(gencb.get(), &ircd::openssl::genprime_cb, arg);

	bignum e{exp};
	call(::RSA_generate_key_ex, &out, bits, e, gencb.get());

	return out;
}

ircd::string_view
ircd::openssl::print(const mutable_buffer &buf,
                     const RSA &rsa,
                     const off_t &offset)
{
	return bio::write(buf, [&rsa, &offset]
	(BIO *const &bio)
	{
		RSA_print(bio, const_cast<RSA *>(&rsa), offset);
	});
}

size_t
ircd::openssl::size(const RSA &key)
{
	#ifdef IRCD_OPENSSL_API_1_0_X
	assert(key.n != nullptr);
	#endif

	return RSA_size(&key);
}

void
ircd::openssl::check(const RSA &key)
{
	if(call<error, -1>(::RSA_check_key, const_cast<RSA *>(&key)) == 0)
		throw error{"Invalid RSA"};
}

bool
ircd::openssl::check(const RSA &key,
                     const std::nothrow_t)
{
	return RSA_check_key(const_cast<RSA *>(&key)) == 1;
}

//
// Envelope
//

void
ircd::openssl::set(EVP_PKEY &out,
                   RSA &in)
{
	call(::EVP_PKEY_set1_RSA, &out, &in);
}

void
ircd::openssl::set(EVP_PKEY &out,
                   EC_KEY &in)
{
	call(::EVP_PKEY_set1_EC_KEY, &out, &in);
}

ircd::string_view
ircd::openssl::write_pem_priv(const mutable_buffer &out,
                              const EVP_PKEY &evp)
{
	EVP_CIPHER *const enc{nullptr};

	uint8_t *const kstr{nullptr};
	const int klen{0};

	pem_password_cb *const pwcb{nullptr};
	void *const u{nullptr};

	auto *const p{const_cast<EVP_PKEY *>(&evp)};
	return bio::write(out, [&evp, &p, &enc, &kstr, &klen, &pwcb, &u]
	(BIO *const &bio)
	{
		switch(EVP_PKEY_type(EVP_PKEY_id(&evp)))
		{
			case EVP_PKEY_RSA:
				call(::PEM_write_bio_RSAPrivateKey, bio, EVP_PKEY_get1_RSA(p), enc, kstr, klen, pwcb, u);
				break;

			case EVP_PKEY_EC:
				call(::PEM_write_bio_ECPrivateKey, bio, EVP_PKEY_get1_EC_KEY(p), enc, kstr, klen, pwcb, u);
				break;

			default:
				call(::PEM_write_bio_PrivateKey, bio, p, enc, kstr, klen, pwcb, u);
				break;
		}
	});
}

ircd::string_view
ircd::openssl::write_pem_pub(const mutable_buffer &out,
                             const EVP_PKEY &evp)
{
	auto *const p{const_cast<EVP_PKEY *>(&evp)};
	return bio::write(out, [&evp, &p]
	(BIO *const &bio)
	{
		switch(EVP_PKEY_type(EVP_PKEY_id(&evp)))
		{
			case EVP_PKEY_RSA:
				call(::PEM_write_bio_RSAPublicKey, bio, EVP_PKEY_get1_RSA(p));
				break;

			case EVP_PKEY_EC:
				call(::PEM_write_bio_EC_PUBKEY, bio, EVP_PKEY_get1_EC_KEY(p));
				break;

			default:
				call(::PEM_write_bio_PUBKEY, bio, p);
				break;
		}
	});
}

EVP_PKEY &
ircd::openssl::read_pem_priv(EVP_PKEY &out_,
                             const string_view &pem)
{
	void *ret{nullptr};
	EVP_PKEY *out{&out_};

	pem_password_cb *const pwcb{nullptr};
	void *const u{nullptr};

	bio::read(pem, [&ret, &out, &pwcb, &u]
	(BIO *const &bio)
	{
		switch(EVP_PKEY_type(EVP_PKEY_id(out)))
		{
			case EVP_PKEY_RSA:
			{
				RSA *rsa(EVP_PKEY_get1_RSA(out));
				ret = PEM_read_bio_RSAPrivateKey(bio, &rsa, pwcb, u);
				call(::EVP_PKEY_set1_RSA, out, rsa);
				break;
			}

			case EVP_PKEY_EC:
			{
				EC_KEY *ec_key(EVP_PKEY_get1_EC_KEY(out));
				ret = PEM_read_bio_ECPrivateKey(bio, &ec_key, pwcb, u);
				EC_KEY_set_asn1_flag(EVP_PKEY_get1_EC_KEY(out), OPENSSL_EC_NAMED_CURVE);
				call(::EVP_PKEY_set1_EC_KEY, out, ec_key);
				break;
			}

			default:
				ret = PEM_read_bio_PrivateKey(bio, &out, pwcb, u);
				break;
		}
	});

	if(unlikely(!ret))
		throw error
		{
			"Failed to read Private Key PEM @ %p (len: %zu)", pem.data(), pem.length()
		};

	return *out;
}

EVP_PKEY &
ircd::openssl::read_pem_pub(EVP_PKEY &out_,
                            const string_view &pem)
{
	void *ret{nullptr};
	EVP_PKEY *out{&out_};

	pem_password_cb *const pwcb{nullptr};
	void *const u{nullptr};

	bio::read(pem, [&ret, &out, &pwcb, &u]
	(BIO *const &bio)
	{
		switch(EVP_PKEY_type(EVP_PKEY_id(out)))
		{
			case EVP_PKEY_RSA:
			{
				RSA *rsa(EVP_PKEY_get1_RSA(out));
				ret = PEM_read_bio_RSAPublicKey(bio, &rsa, pwcb, u);
				call(::EVP_PKEY_set1_RSA, out, rsa);
				break;
			}

			case EVP_PKEY_EC:
			{
				EC_KEY *ec_key(EVP_PKEY_get1_EC_KEY(out));
				ret = PEM_read_bio_EC_PUBKEY(bio, &ec_key, pwcb, u);
				EC_KEY_set_asn1_flag(ec_key, OPENSSL_EC_NAMED_CURVE);
				call(::EVP_PKEY_set1_EC_KEY, out, ec_key);
				break;
			}

			default:
				ret = PEM_read_bio_PUBKEY(bio, &out, pwcb, u);
				break;
		}
	});

	if(unlikely(!ret))
		throw error
		{
			"Failed to read Public Key PEM @ %p (len: %zu)", pem.data(), pem.length()
		};

	return *out;
}

//
// lib generale
//

void
ircd::openssl::clear_error()
{
	ERR_clear_error();
}

ulong
ircd::openssl::get_error()
{
	return ERR_get_error();
}

ulong
ircd::openssl::peek_error()
{
	return ERR_peek_error();
}

ircd::string_view
ircd::openssl::error_string(const mutable_buffer &buf,
                            const ulong &e)
{
	ERR_error_string_n(e, data(buf), size(buf));
	return { data(buf), strnlen(data(buf), size(buf)) };
}

//
// bio
//

void
ircd::openssl::bio::read_file(const string_view &path,
                              const cb_closure &closure)
{
	const size_t size
	{
		fs::size(path)
	};

	#ifdef IRCD_OPENSSL_API_1_1_X
	const custom_ptr<void> buf
	{
		OPENSSL_secure_malloc(size), [&size]
		(void *const buf)
		{
			OPENSSL_secure_free(buf);
		}
	};
	#else
	const custom_ptr<void> buf
	{
		OPENSSL_malloc_locked(size), [&size]
		(void *const buf)
		{
			OPENSSL_cleanse(buf, size);
			OPENSSL_free_locked(buf);
		}
	};
	#endif

	const mutable_buffer mb
	{
		reinterpret_cast<char *>(buf.get()), size
	};

	closure(fs::read(path, mb));
}

void
ircd::openssl::bio::write_file(const string_view &path,
                               const mb_closure &closure,
                               const size_t &size)
{
	#ifdef IRCD_OPENSSL_API_1_1_X
	const custom_ptr<void> buf
	{
		OPENSSL_secure_malloc(size), [&size]
		(void *const buf)
		{
			OPENSSL_secure_free(buf);
		}
	};
	#else
	const custom_ptr<void> buf
	{
		OPENSSL_malloc_locked(size), [&size]
		(void *const buf)
		{
			OPENSSL_cleanse(buf, size);
			OPENSSL_free_locked(buf);
		}
	};
	#endif

	const mutable_buffer mb
	{
		reinterpret_cast<char *>(buf.get()), size
	};

	fs::overwrite(path, closure(mb));
}

void
ircd::openssl::bio::read(const const_buffer &buf,
                         const closure &closure)
{
	const custom_ptr<BIO> bp
	{
// OpenSSL branch
#if !defined(LIBRESSL_VERSION_NUMBER)
		BIO_new_mem_buf(data(buf), size(buf)), BIO_free
// LibreSSL branch
#else
		BIO_new_mem_buf((void *)data(buf), size(buf)), BIO_free
#endif
	};

	closure(bp.get());
}

ircd::string_view
ircd::openssl::bio::write(const mutable_buffer &buf,
                          const closure &closure)
{
	const custom_ptr<BIO> bp
	{
		BIO_new(BIO_s_mem()), BIO_free
	};

	//TODO: XXX: BAD: if the buffer is too small:
	// I saw this try to realloc() our buffer. It did not respect
	// the max size. I'd expect either truncation or error, so wtf?

	BUF_MEM bm {0};
	bm.data = data(buf);
	bm.max = size(buf);
	call(::BIO_ctrl, bp.get(), BIO_C_SET_BUF_MEM, BIO_NOCLOSE, &bm);

	closure(bp.get());

	assert(size_t(bm.length) <= size(buf));
	return { data(buf), size_t(bm.length) };
}

//
// bignum
//

ircd::string_view
ircd::openssl::u2a(const mutable_buffer &out,
                   const BIGNUM *const &a)
{
	const unique_buffer<mutable_buffer> tmp
	{
		size(a)
	};

	return ircd::u2a(out, data(tmp, a));
}

ircd::mutable_buffer
ircd::openssl::data(const mutable_buffer &out,
                    const BIGNUM *const &a)
{
	if(!a)
		return { data(out), 0UL };

	if(unlikely(size(out) < size(a)))
		throw buffer_error
		{
			"buffer size %zu short for BIGNUM of size %zu", size(out), size(a)
		};

	const auto len
	{
		BN_bn2bin(a, reinterpret_cast<uint8_t *>(data(out)))
	};

	reverse(out);
	assert(len <= ssize_t(size(out)));
	return { data(out), size_t(len) };
}

size_t
ircd::openssl::size(const BIGNUM *const &a)
{
	return BN_num_bytes(a);
}

//
// bignum::bignum
//

ircd::openssl::bignum::bignum(const uint128_t &val)
:bignum
{
	const_buffer
	{
		reinterpret_cast<const char *>(&val), sizeof(val)
	}
}
{
}

ircd::openssl::bignum::bignum(const const_buffer &bin)
:a{[&bin]
{
	// Our binary buffer is little endian.
	thread_local char tmp[64_KiB];
	const critical_assertion ca;
	const size_t buf_size
	{
		#if defined(__HAVE_BUILTIN_SPECULATION_SAFE_VALUE)
			__builtin_speculation_safe_value(size(bin))
		#else
			size(bin)
		#endif
	};

	const mutable_buffer buf{tmp, buf_size};
	if(unlikely(size(bin) > sizeof(tmp)))
		throw buffer_error
		{
			"buffer input of %zu for bignum > tmp %zu",
			size(bin),
			sizeof(tmp)
		};

	reverse(buf, bin);
	return BN_bin2bn(reinterpret_cast<uint8_t *>(data(buf)), size(buf), nullptr);
}()}
{
	if(unlikely(!a))
		throw error
		{
			"Error creating bignum from binary buffer..."
		};
}

ircd::openssl::bignum::bignum(const BIGNUM &a)
:a{BN_dup(&a)}
{
}

ircd::openssl::bignum::bignum(const bignum &o)
:a{BN_dup(o.a)}
{
}

ircd::openssl::bignum::bignum(bignum &&o)
noexcept
:a{std::move(o.a)}
{
	o.a = nullptr;
}

ircd::openssl::bignum &
ircd::openssl::bignum::operator=(const bignum &o)
{
	if(unlikely(!BN_copy(a, o.a)))
		throw error
		{
			"Failed to copy bignum from %p to %p", &o, this
		};

	return *this;
}

ircd::openssl::bignum &
ircd::openssl::bignum::operator=(bignum &&o)
noexcept
{
	this->~bignum();
	a = std::move(o.a);
	o.a = nullptr;
	return *this;
}

ircd::openssl::bignum::~bignum()
noexcept
{
	BN_free(a);
}

ircd::openssl::bignum::operator
ircd::uint128_t()
const
{
	uint128_t ret{0};
	const mutable_buffer buf
	{
		reinterpret_cast<char *>(&ret), sizeof(ret)
	};

	data(buf, a);
	return ret;
}

ircd::openssl::bignum::operator
BIGNUM &()
{
	assert(a != nullptr);
	return *a;
}

ircd::openssl::bignum::operator
BIGNUM *const &()
{
	return a;
}

ircd::openssl::bignum::operator
BIGNUM **()
{
	return &a;
}

ircd::openssl::bignum::operator
const BIGNUM &()
const
{
	assert(a != nullptr);
	return *a;
}

ircd::openssl::bignum::operator
const BIGNUM *()
const
{
	return a;
}

size_t
ircd::openssl::bignum::bytes()
const
{
	return BN_num_bytes(get());
}

size_t
ircd::openssl::bignum::bits()
const
{
	return BN_num_bits(get());
}

BIGNUM *
ircd::openssl::bignum::release()
{
	BIGNUM *const a{this->a};
	this->a = nullptr;
	return a;
}

BIGNUM *
ircd::openssl::bignum::get()
{
	return a;
}

const BIGNUM *
ircd::openssl::bignum::get()
const
{
	return a;
}

//
// init
//

ircd::openssl::init::init()
{
	if(long(version_api) != long(version_abi))
		log::warning
		{
			"Linked OpenSSL version '%s' is not the compiled OpenSSL version '%s'",
			string_view{version_api},
			string_view{version_abi},
		};

	OPENSSL_init();
	ERR_load_crypto_strings();
	ERR_load_ERR_strings();
	ec_init();

/*
	const auto their_id_callback
	{
		CRYPTO_THREADID_get_callback()
	};

	assert(their_id_callback == nullptr);
	CRYPTO_THREADID_set_callback(locking::id_callback);
*/

/*
	const auto their_locking_callback
	{
		CRYPTO_get_locking_callback()
	};

	if(their_locking_callback)
		throw error("Overwrite their locking callback @ %p ???",
		            their_locking_callback);

	CRYPTO_set_locking_callback(locking::callback);
*/
}

ircd::openssl::init::~init()
noexcept
{
	ec_fini();

	//assert(CRYPTO_get_locking_callback() == locking::callback);
	//assert(CRYPTO_THREADID_get_callback() == locking::id_callback);

	ERR_free_strings();
}

///////////////////////////////////////////////////////////////////////////////
//
// crh.h
//

//
// hmac
//

#ifdef IRCD_OPENSSL_API_1_0_X
struct ircd::crh::hmac::ctx
:HMAC_CTX
{
	static constexpr const size_t &MAX_CTXS {64};
	static thread_local allocator::fixed<ctx, MAX_CTXS> ctxs;

	static void *operator new(const size_t count);
	static void operator delete(void *const ptr, const size_t count);

	ctx(const string_view &algorithm, const const_buffer &key);
	~ctx() noexcept;
};
#else
struct ircd::crh::hmac::ctx
:custom_ptr<HMAC_CTX>
{
	static constexpr const size_t &MAX_CTXS {64};
	static thread_local allocator::fixed<ctx, MAX_CTXS> ctxs;

	static void *operator new(const size_t count);
	static void operator delete(void *const ptr, const size_t count);

	ctx(const string_view &algorithm, const const_buffer &key);
	~ctx() noexcept;
};
#endif

decltype(ircd::crh::hmac::ctx::ctxs)
thread_local ircd::crh::hmac::ctx::ctxs
{};

void *
ircd::crh::hmac::ctx::operator new(const size_t bytes)
{
	assert(bytes > 0);
	assert(bytes % sizeof(ctx) == 0);
	return ctxs().allocate(bytes / sizeof(ctx));
}

void
ircd::crh::hmac::ctx::operator delete(void *const ptr,
                                      const size_t bytes)
{
	if(!ptr)
		return;

	assert(bytes % sizeof(ctx) == 0);
	ctxs().deallocate(reinterpret_cast<ctx *>(ptr), bytes / sizeof(ctx));
}

//
// hmac::ctx::ctx
//

ircd::crh::hmac::ctx::ctx(const string_view &algorithm,
                          const const_buffer &key)
#ifdef IRCD_OPENSSL_API_1_0_X
:HMAC_CTX{0}
#else
:custom_ptr<HMAC_CTX>
{
	HMAC_CTX_new(), HMAC_CTX_free
}
#endif
{
	const EVP_MD *const md
	{
		iequals(algorithm, "sha1")?
			EVP_sha1():
		iequals(algorithm, "sha256")?
			EVP_sha256():
			nullptr
	};

	if(unlikely(!md))
		throw error
		{
			"Algorithm '%s' not supported for HMAC", algorithm
		};

	#ifdef IRCD_OPENSSL_API_1_0_X
	HMAC_CTX_init(this);
	openssl::call(::HMAC_Init_ex, this, data(key), size(key), md, nullptr);
	#else
	openssl::call(::HMAC_Init_ex, this->get(), data(key), size(key), md, nullptr);
	#endif
}

ircd::crh::hmac::ctx::~ctx()
noexcept
{
	#ifdef IRCD_OPENSSL_API_1_0_X
	HMAC_CTX_cleanup(this);
	#endif
}

//
// hmac::hmac
//

ircd::crh::hmac::hmac(const string_view &algorithm,
                      const const_buffer &key)
:ctx
{
	std::make_unique<struct ctx>(algorithm, key)
}
{
}

ircd::crh::hmac::~hmac()
noexcept
{
}

void
ircd::crh::hmac::update(const const_buffer &buf)
{
	assert(bool(ctx));
	const auto ptr
	{
		reinterpret_cast<const uint8_t *>(data(buf))
	};

	#ifdef IRCD_OPENSSL_API_1_0_X
	openssl::call(::HMAC_Update, ctx.get(), ptr, size(buf));
	#else
	openssl::call(::HMAC_Update, ctx->get(), ptr, size(buf));
	#endif
}

ircd::const_buffer
ircd::crh::hmac::finalize(const mutable_buffer &buf)
{
	assert(bool(ctx));
	const auto ptr
	{
		reinterpret_cast<uint8_t *>(data(buf))
	};

	uint len;

	#ifdef IRCD_OPENSSL_API_1_0_X
	openssl::call(::HMAC_Final, ctx.get(), ptr, &len);
	#else
	openssl::call(::HMAC_Final, ctx->get(), ptr, &len);
	#endif

	return {data(buf), len};
}

size_t
ircd::crh::hmac::length()
const
{
	assert(bool(ctx));

	#ifdef IRCD_OPENSSL_API_1_0_X
	return HMAC_size(ctx.get());
	#else
	return HMAC_size(ctx->get());
	#endif
}

//
// sha1
//

namespace ircd::crh
{
	static void finalize(struct sha1::ctx *const &, const mutable_buffer &);
}

struct ircd::crh::sha1::ctx
:SHA_CTX
{
	static constexpr const size_t &MAX_CTXS {64};
	static thread_local allocator::fixed<ctx, MAX_CTXS> ctxs;

	static void *operator new(const size_t count);
	static void operator delete(void *const ptr, const size_t count);

	ctx();
	~ctx() noexcept;
};

decltype(ircd::crh::sha1::ctx::ctxs)
thread_local ircd::crh::sha1::ctx::ctxs
{};

void *
ircd::crh::sha1::ctx::operator new(const size_t bytes)
{
	assert(bytes > 0);
	assert(bytes % sizeof(ctx) == 0);
	return ctxs().allocate(bytes / sizeof(ctx));
}

void
ircd::crh::sha1::ctx::operator delete(void *const ptr,
                                      const size_t bytes)
{
	if(!ptr)
		return;

	assert(bytes % sizeof(ctx) == 0);
	ctxs().deallocate(reinterpret_cast<ctx *>(ptr), bytes / sizeof(ctx));
}

//
// sha1::ctx::ctx
//

ircd::crh::sha1::ctx::ctx()
{
	openssl::call(::SHA1_Init, this);
}

ircd::crh::sha1::ctx::~ctx()
noexcept
{
}

//
// sha1::sha1
//

ircd::crh::sha1::sha1()
:ctx{std::make_unique<struct ctx>()}
{
}

/// One-shot functor. Immediately calls update(); no output
ircd::crh::sha1::sha1(const const_buffer &in)
:sha1{}
{
	update(in);
}

/// One-shot functor. Immediately calls operator(). NOTE: This hashing context
/// cannot be used again after this ctor.
ircd::crh::sha1::sha1(const mutable_buffer &out,
                      const const_buffer &in)
:sha1{}
{
	operator()(out, in);
}

ircd::crh::sha1::~sha1()
noexcept
{
}

void
ircd::crh::sha1::update(const const_buffer &buf)
{
	assert(bool(ctx));
	openssl::call(::SHA1_Update, ctx.get(), data(buf), size(buf));
}

void
ircd::crh::sha1::digest(const mutable_buffer &buf)
const
{
	assert(bool(ctx));
	auto copy(*ctx);
	crh::finalize(&copy, buf);
}

void
ircd::crh::sha1::finalize(const mutable_buffer &buf)
{
	assert(bool(ctx));
	crh::finalize(ctx.get(), buf);
}

size_t
ircd::crh::sha1::length()
const
{
	return digest_size;
}

void
ircd::crh::finalize(struct sha1::ctx *const &ctx,
                    const mutable_buffer &buf)
{
	assert(size(buf) >= sha1::digest_size);
	uint8_t *const md
	{
		reinterpret_cast<uint8_t *>(data(buf))
	};

	openssl::call(::SHA1_Final, md, ctx);
}

//
// sha256
//

namespace ircd::crh
{
	static void finalize(struct sha256::ctx *const &, const mutable_buffer &);
}

struct ircd::crh::sha256::ctx
:SHA256_CTX
{
	static constexpr const size_t &MAX_CTXS {64};
	static thread_local allocator::fixed<ctx, MAX_CTXS> ctxs;

	static void *operator new(const size_t count);
	static void operator delete(void *const ptr, const size_t count);

	ctx();
	~ctx() noexcept;
};

decltype(ircd::crh::sha256::ctx::ctxs)
thread_local ircd::crh::sha256::ctx::ctxs
{};

void *
ircd::crh::sha256::ctx::operator new(const size_t bytes)
{
	assert(bytes > 0);
	assert(bytes % sizeof(ctx) == 0);
	return ctxs().allocate(bytes / sizeof(ctx));
}

void
ircd::crh::sha256::ctx::operator delete(void *const ptr,
                                        const size_t bytes)
{
	if(!ptr)
		return;

	assert(bytes % sizeof(ctx) == 0);
	ctxs().deallocate(reinterpret_cast<ctx *>(ptr), bytes / sizeof(ctx));
}

//
// sha256::ctx::ctx
//

ircd::crh::sha256::ctx::ctx()
{
	openssl::call(::SHA256_Init, this);
}

ircd::crh::sha256::ctx::~ctx()
noexcept
{
}

//
// sha256::sha256
//

ircd::crh::sha256::sha256()
:ctx{std::make_unique<struct ctx>()}
{
}

/// One-shot functor. Immediately calls update(); no output
ircd::crh::sha256::sha256(const const_buffer &in)
:sha256{}
{
	update(in);
}

/// One-shot functor. Immediately calls operator(). NOTE: This hashing context
/// cannot be used again after this ctor.
ircd::crh::sha256::sha256(const mutable_buffer &out,
                          const const_buffer &in)
:sha256{}
{
	operator()(out, in);
}

ircd::crh::sha256::~sha256()
noexcept
{
}

void
ircd::crh::sha256::update(const const_buffer &buf)
{
	assert(bool(ctx));
	openssl::call(::SHA256_Update, ctx.get(), data(buf), size(buf));
}

void
ircd::crh::sha256::digest(const mutable_buffer &buf)
const
{
	assert(bool(ctx));
	auto copy(*ctx);
	crh::finalize(&copy, buf);
}

void
ircd::crh::sha256::finalize(const mutable_buffer &buf)
{
	assert(bool(ctx));
	crh::finalize(ctx.get(), buf);
}

size_t
ircd::crh::sha256::length()
const
{
	return digest_size;
}

void
ircd::crh::finalize(struct sha256::ctx *const &ctx,
                    const mutable_buffer &buf)
{
	assert(size(buf) >= sha256::digest_size);
	uint8_t *const md
	{
		reinterpret_cast<uint8_t *>(data(buf))
	};

	openssl::call(::SHA256_Final, md, ctx);
}

//
// ripemd160
//

namespace ircd::crh
{
	static void finalize(struct ripemd160::ctx *const &, const mutable_buffer &);
}

struct ircd::crh::ripemd160::ctx
:RIPEMD160_CTX
{
	static constexpr const size_t &MAX_CTXS {64};
	static thread_local allocator::fixed<ctx, MAX_CTXS> ctxs;

	static void *operator new(const size_t count);
	static void operator delete(void *const ptr, const size_t count);

	ctx();
	~ctx() noexcept;
};

decltype(ircd::crh::ripemd160::ctx::ctxs)
thread_local ircd::crh::ripemd160::ctx::ctxs
{};

void *
ircd::crh::ripemd160::ctx::operator new(const size_t bytes)
{
	assert(bytes > 0);
	assert(bytes % sizeof(ctx) == 0);
	return ctxs().allocate(bytes / sizeof(ctx));
}

void
ircd::crh::ripemd160::ctx::operator delete(void *const ptr,
                                           const size_t bytes)
{
	if(!ptr)
		return;

	assert(bytes % sizeof(ctx) == 0);
	ctxs().deallocate(reinterpret_cast<ctx *>(ptr), bytes / sizeof(ctx));
}

//
// ripemd160::ctx::ctx
//

ircd::crh::ripemd160::ctx::ctx()
{
	openssl::call(::RIPEMD160_Init, this);
}

ircd::crh::ripemd160::ctx::~ctx()
noexcept
{
}

//
// ripemd160::ripemd160
//

ircd::crh::ripemd160::ripemd160()
:ctx{std::make_unique<struct ctx>()}
{
}

/// One-shot functor. Immediately calls update(); no output
ircd::crh::ripemd160::ripemd160(const const_buffer &in)
:ripemd160{}
{
	update(in);
}

/// One-shot functor. Immediately calls operator(). NOTE: This hashing context
/// cannot be used again after this ctor.
ircd::crh::ripemd160::ripemd160(const mutable_buffer &out,
                                const const_buffer &in)
:ripemd160{}
{
	operator()(out, in);
}

ircd::crh::ripemd160::~ripemd160()
noexcept
{
}

void
ircd::crh::ripemd160::update(const const_buffer &buf)
{
	assert(bool(ctx));
	openssl::call(::RIPEMD160_Update, ctx.get(), data(buf), size(buf));
}

void
ircd::crh::ripemd160::digest(const mutable_buffer &buf)
const
{
	assert(bool(ctx));
	auto copy(*ctx);
	crh::finalize(&copy, buf);
}

void
ircd::crh::ripemd160::finalize(const mutable_buffer &buf)
{
	assert(bool(ctx));
	crh::finalize(ctx.get(), buf);
}

size_t
ircd::crh::ripemd160::length()
const
{
	return digest_size;
}

void
ircd::crh::finalize(struct ripemd160::ctx *const &ctx,
                    const mutable_buffer &buf)
{
	assert(size(buf) >= ripemd160::digest_size);
	uint8_t *const md
	{
		reinterpret_cast<uint8_t *>(data(buf))
	};

	openssl::call(::RIPEMD160_Final, md, ctx);
}

///////////////////////////////////////////////////////////////////////////////
//
// Internal section for OpenSSL locking.
//
// This is delicate because we really shouldn't need this, and as a library it
// is not nice to other libraries to assume this interface for ourselves.
// Nevertheless, I have specified it here foremost for debugging and if at some
// point in the future we really require it.
//

namespace ircd::openssl::locking
{
	const int READ_LOCK     { CRYPTO_LOCK + CRYPTO_READ    };
	const int WRITE_LOCK    { CRYPTO_LOCK + CRYPTO_WRITE   };
	const int READ_UNLOCK   { CRYPTO_UNLOCK + CRYPTO_READ  };
	const int WRITE_UNLOCK  { CRYPTO_UNLOCK + CRYPTO_WRITE };

	std::vector<std::shared_mutex> mutex
	{
		CRYPTO_num_locks() >= 0?
			size_t(CRYPTO_num_locks()): 0UL
	};

	static ircd::string_view reflect(const int &mode);
	static std::string debug(const int, const int, const char *const, const int);
	static void callback(const int, const int, const char *const, const int) noexcept;
	static void id_callback(CRYPTO_THREADID *const tid) noexcept;
}

void
ircd::openssl::locking::id_callback(CRYPTO_THREADID *const tid)
noexcept try
{
	const auto ttid
	{
		std::this_thread::get_id()
	};

	const auto otid
	{
		uint32_t(std::hash<std::thread::id>{}(ttid)) % std::numeric_limits<uint32_t>::max()
	};

//	log::debug("OpenSSL thread id callback: setting %p to %u",
//	           (const void *)tid,
//	           otid);

	CRYPTO_THREADID_set_numeric(tid, otid);
}
catch(const std::exception &e)
{
	log::critical("OpenSSL thread id callback (tid=%p): %s",
	              (const void *)tid,
	              e.what());

	ircd::terminate();
}

void
ircd::openssl::locking::callback(const int mode,
                                 const int num,
                                 const char *const file,
                                 const int line)
noexcept try
{
	log::debug("OpenSSL: %s", debug(mode, num, file, line));

	auto &mutex
	{
		locking::mutex[num]
	};

	switch(mode)
	{
		case CRYPTO_LOCK:
		case WRITE_LOCK:     mutex.lock();            break;
		case READ_LOCK:      mutex.lock_shared();     break;
		case CRYPTO_UNLOCK:
		case WRITE_UNLOCK:   mutex.unlock();          break;
		case READ_UNLOCK:    mutex.unlock_shared();   break;
	}
}
catch(const std::exception &e)
{
	log::critical("OpenSSL locking callback (%s): %s",
	              debug(mode, num, file, line),
	              e.what());

	ircd::terminate();
}

std::string
ircd::openssl::locking::debug(const int mode,
                              const int num,
                              const char *const file,
                              const int line)
{
	return fmt::snstringf
	{
		1024, "[%02d] %-15s main thread: %d ctx: %u %s %d",
		num,
		reflect(mode),
		is_main_thread(),
		ctx::id(),
		file,
		line
	};
}

ircd::string_view
ircd::openssl::locking::reflect(const int &mode)
{
	switch(mode)
	{
		case CRYPTO_LOCK:    return "LOCK";
		case WRITE_LOCK:     return "WRITE_LOCK";
		case READ_LOCK:      return "READ_LOCK";
		case CRYPTO_UNLOCK:  return "UNLOCK";
		case WRITE_UNLOCK:   return "WRITE_UNLOCK";
		case READ_UNLOCK:    return "READ_UNLOCK";
	}

	return "?????";
}

//
// internal util
//

// This callback can be used to integrate generating with ircd::ctx
// or ctx::offload/thread or some status update. For now we just eat
// the milliseconds of prime generation on main.
// return false causes call(RSA_generate_key_ex) to throw
int
ircd::openssl::genprime_cb(const int stat,
                           const int ith,
                           BN_GENCB *const ctx)
noexcept try
{
	assert(ctx != nullptr);

	#ifdef IRCD_OPENSSL_API_1_0_X
	auto &arg{ctx->arg};
	#else
	auto *const &arg(BN_GENCB_get_arg(ctx));
	#endif

	const auto yield_point{[]
	{
		if(ctx::current)
			ctx::yield();
	}};

	switch(stat)
	{
		case 0: // generating i-th potential prime
			return true;

		case 1: // testing i-th potential prime
		{
			yield_point();
			return true;
		}

		case 2: // found i-th potential prime but rejected for RSA
		{
			yield_point();
			return true;
		}

		case 3: switch(ith) // found for RSA...
		{
			case 0: // found P
				return true;

			case 1: // found Q
				return true;

			default:
				return false;
		}

		default:
			return false;
	}
}
catch(...)
{
	return false;
}

//
// call()
//

template<class exception,
         int ERR_CODE,
         class function,
         class... args>
static int
ircd::openssl::call(function&& f,
                    args&&... a)
{
	const auto ret
	{
		f(std::forward<args>(a)...)
	};

	if(unlikely(ret == ERR_CODE))
		throw_error<exception>();

	return ret;
}

template<class exception>
static void
ircd::openssl::throw_error(const unsigned long &code)
{
	const auto &msg
	{
		ERR_reason_error_string(code)?: "UNKNOWN ERROR"
	};

	throw exception
	{
		"OpenSSL #%lu: %s", code, msg
	};
}

template<class exception>
static void
ircd::openssl::throw_error()
{
	const auto code
	{
		get_error()
	};

	throw_error(code);
}

#if defined(LIBRESSL_VERSION_NUMBER)
///////////////////////////////////////////////////////////////////////////////
//
// Contributed by Danilo Spinella (DanySpin97) for LibreSSL.
// Author credits to TJ Saunders (Castaglia):
// https://github.com/proftpd/proftpd/commit/a3d65e868
//

/* We need to provide our own backport of the ASN1_TIME_diff() function. */
static time_t ASN1_TIME_seconds(const ASN1_TIME *a) {
  static const int min[9] = { 0, 0, 1, 1, 0, 0, 0, 0, 0 };
  static const int max[9] = { 99, 99, 12, 31, 23, 59, 59, 12, 59 };
  time_t t = 0;
  char *text;
  int text_len;
  int i, j, n;
  unsigned int nyears, nmons, nhours, nmins, nsecs;
   if (a->type != V_ASN1_GENERALIZEDTIME) {
    return 0;
  }
   text_len = a->length;
  text = (char *) a->data;
   /* GENERALIZEDTIME is similar to UTCTIME except the year is represented
   * as YYYY. This stuff treats everything as a two digit field so make
   * first two fields 00 to 99
   */
   if (text_len < 13) {
    return 0;
  }
   nyears = nmons = nhours = nmins = nsecs = 0;
   for (i = 0, j = 0; i < 7; i++) {
    if (i == 6 &&
        (text[j] == 'Z' ||
         text[j] == '+' ||
         text[j] == '-')) {
      i++;
      break;
    }
     if (text[j] < '0' ||
        text[j] > '9') {
      return 0;
    }
     n = text[j] - '0';
    if (++j > text_len) {
      return 0;
    }
     if (text[j] < '0' ||
        text[j] > '9') {
      return 0;
    }
     n = (n * 10) + (text[j] - '0');
    if (++j > text_len) {
      return 0;
    }
     if (n < min[i] ||
        n > max[i]) {
      return 0;
    }
     switch (i) {
      case 0:
        /* Years */
        nyears = (n * 100);
        break;
       case 1:
        /* Years */
        nyears += n;
        break;
       case 2:
        /* Month */
        nmons = n - 1;
        break;
       case 3:
        /* Day of month; ignored */
        break;
       case 4:
        /* Hours */
        nhours = n;
        break;
       case 5:
        /* Minutes */
        nmins = n;
        break;
       case 6:
        /* Seconds */
        nsecs = n;
        break;
    }
  }
   /* Yes, this is not calendrical accurate.  It only needs to be a good
   * enough estimation, as it is used (currently) only for determining the
   * validity window of an OCSP request (in seconds).
   */
  t = (nyears * 365 * 86400) + (nmons * 30 * 86400) * (nhours * 3600) + nsecs;
   /* Optional fractional seconds: decimal point followed by one or more
   * digits.
   */
  if (text[j] == '.') {
    if (++j > text_len) {
      return 0;
    }
     i = j;
     while (text[j] >= '0' &&
           text[j] <= '9' &&
           j <= text_len) {
      j++;
    }
     /* Must have at least one digit after decimal point */
    if (i == j) {
      return 0;
    }
  }
   if (text[j] == 'Z') {
    j++;
   } else if (text[j] == '+' ||
             text[j] == '-') {
    int offsign, offset = 0;
     offsign = text[j] == '-' ? -1 : 1;
    j++;
     if (j + 4 > text_len) {
      return 0;
    }
     for (i = 7; i < 9; i++) {
      if (text[j] < '0' ||
          text[j] > '9') {
        return 0;
      }
       n = text[j] - '0';
      j++;
       if (text[j] < '0' ||
          text[j] > '9') {
        return 0;
      }
       n = (n * 10) + text[j] - '0';
       if (n < min[i] ||
          n > max[i]) {
        return 0;
      }
       if (i == 7) {
        offset = n * 3600;
       } else if (i == 8) {
        offset += n * 60;
      }
       j++;
    }
     if (offset > 0) {
      t += (offset * offsign);
    }
   } else if (text[j]) {
    /* Missing time zone information. */
    return 0;
  }
   return t;
}

static int ASN1_TIME_diff(int *pday, int *psec, const ASN1_TIME *from,
    const ASN1_TIME *to) {
  time_t from_secs, to_secs, diff_secs;
  long diff_days;
   from_secs = ASN1_TIME_seconds(from);
  if (from_secs == 0) {
    return 0;
  }
   to_secs = ASN1_TIME_seconds(to);
  if (to_secs == 0) {
    return 0;
  }
   if (to_secs > from_secs) {
    diff_secs = to_secs - from_secs;
   } else {
    diff_secs = from_secs - to_secs;
  }
   /* The ASN1_TIME_diff() API in OpenSSL-1.0.2+ offers days and seconds,
   * possibly to handle LARGE time differences without overflowing the data
   * type for seconds.  So we do the same.
   */
   diff_days = diff_secs % 86400;
  diff_secs -= (diff_days * 86400);
   if (pday) {
    *pday = (int) diff_days;
  }
   if (psec) {
    *psec = diff_secs;
  }
   return 1;
}

#endif /* Before OpenSSL-1.0.2 */
