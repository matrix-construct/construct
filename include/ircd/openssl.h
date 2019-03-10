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
#define HAVE_IRCD_OPENSSL_H

// Forward declarations for OpenSSL because it is not included here. Note
// that these are declared in the extern namespace outside of ircd:: but
// match those in the OpenSSL headers and should not be too much trouble.
struct ssl_st;
struct ssl_ctx_st;
struct ssl_cipher_st;
struct rsa_st;
struct x509_st;
struct x509_store_ctx_st;
struct bignum_st;
struct bignum_ctx;
struct bio_st;
struct evp_pkey_st;
struct ec_group_st;
struct ec_point_st;
struct ec_key_st;
struct dh_st;

/// OpenSSL library interface. Provides things we need to expose from OpenSSL
/// to the rest of the project.
namespace ircd::openssl
{
	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, buffer_error)

	struct init;
	struct bignum;

	// typedef analogues
	using SSL = ::ssl_st;
	using SSL_CTX = ::ssl_ctx_st;
	using SSL_CIPHER = ::ssl_cipher_st;
	using RSA = ::rsa_st;
	using X509 = ::x509_st;
	using X509_STORE_CTX = ::x509_store_ctx_st;
	using BIGNUM = ::bignum_st;
	using BN_CTX = ::bignum_ctx;
	using EVP_PKEY = ::evp_pkey_st;
	using BIO = ::bio_st;
	using EC_GROUP = ::ec_group_st;
	using EC_POINT = ::ec_point_st;
	using EC_KEY = ::ec_key_st;
	using DH = ::dh_st;

	// Header version; library version
	std::pair<string_view, string_view> version();

	// Observers
	string_view error_string(const mutable_buffer &buf, const ulong &);
	ulong peek_error();

	// Using these will clobber other libraries (like boost); so don't
	ulong get_error();
	void clear_error();

	// Envelope suite
	EVP_PKEY &read_pem_pub(EVP_PKEY &out, const string_view &pem);
	EVP_PKEY &read_pem_priv(EVP_PKEY &out, const string_view &pem);
	string_view write_pem_pub(const mutable_buffer &out, const EVP_PKEY &);
	string_view write_pem_priv(const mutable_buffer &out, const EVP_PKEY &);
	void set(EVP_PKEY &out, RSA &in);
	void set(EVP_PKEY &out, EC_KEY &in);

	// RSA suite
	void check(const RSA &);
	bool check(const RSA &, std::nothrow_t);
	size_t size(const RSA &); // RSA_size() / mod size in bytes
	string_view print(const mutable_buffer &buf, const RSA &, const off_t &offset = 0);
	RSA &genrsa(RSA &out, const uint &bits = 2048, const uint &e = 0x10001);
	void genrsa(const string_view &skfile, const string_view &pkfile, const json::object &opts = {});

	// EC suite
	extern const EC_GROUP *secp256k1;
	void check(const EC_KEY &);
	bool check(const EC_KEY &, const std::nothrow_t);
	string_view print(const mutable_buffer &buf, const EC_KEY &, const off_t &offset = 0);
	void genec(const string_view &skfile, const string_view &pkfile, const EC_GROUP *const & = secp256k1);

	// DH suite
	extern const size_t DH_DEFAULT_GEN;
	extern const size_t DH_DEFAULT_BITS;
	extern const string_view rfc3526_dh_params_pem;
	DH &gendh(DH &, const uint &bits = DH_DEFAULT_BITS, const uint &gen = DH_DEFAULT_GEN);
	string_view gendh(const mutable_buffer &, const uint &bits = DH_DEFAULT_BITS, const uint &gen = DH_DEFAULT_GEN);
	void gendh(const string_view &dhfile, const uint &bits = DH_DEFAULT_BITS, const uint &gen = DH_DEFAULT_GEN);

	// X.509 suite
	const_buffer i2d(const mutable_buffer &out, const X509 &);
	const_buffer cert2d(const mutable_buffer &out, const string_view &pem);
	X509 &read_pem(X509 &out, const string_view &pem);
	string_view write_pem(const mutable_buffer &out, const X509 &);
	string_view print(const mutable_buffer &buf, const X509 &, ulong flags = -1);
	string_view printX509(const mutable_buffer &buf, const string_view &pem, ulong flags = -1);
	string_view genX509(const mutable_buffer &out, EVP_PKEY &, const json::object &opts);
	string_view genX509_rsa(const mutable_buffer &out, const json::object &opts);
	string_view genX509_ec(const mutable_buffer &out, const json::object &opts);
	string_view print_subject(const mutable_buffer &buf, const X509 &, ulong flags = -1);
	string_view print_subject(const mutable_buffer &buf, const string_view &pem, ulong flags = -1);
	string_view subject_common_name(const mutable_buffer &out, const X509 &);
	time_t not_before(const X509 &);
	time_t not_after(const X509 &);
	std::string stringify(const X509 &);
	const X509 &peer_cert(const SSL &);
	X509 &peer_cert(SSL &);

	int get_error(const X509_STORE_CTX &);
	const char *cert_error_string(const long &);
	const char *get_error_string(const X509_STORE_CTX &);
	uint get_error_depth(const X509_STORE_CTX &);
	const X509 &current_cert(const X509_STORE_CTX &);
	X509 &current_cert(X509_STORE_CTX &);

	// SSL suite
	string_view name(const SSL_CIPHER &);
	const SSL_CIPHER *current_cipher(const SSL &);
	string_view shared_ciphers(const mutable_buffer &buf, const SSL &);
	string_view cipher_list(const SSL &, const int &priority = -1);
	void set_cipher_list(SSL &, const std::string &list);
	void set_cipher_list(SSL_CTX &, const std::string &list);
	void set_ecdh_auto(SSL_CTX &, const bool & = true);
	void set_ecdh_auto(SSL &, const bool & = true);
	void set_tmp_ecdh(SSL_CTX &, EC_KEY &);
	void set_curves(SSL_CTX &, std::string list);
	void set_curves(SSL &, std::string list);
}

/// OpenSSL BIO convenience utils and wraps; also secure file IO closures
namespace ircd::openssl::bio
{
	// Presents a memory BIO closure hiding boilerplate
	using closure = std::function<void (BIO *const &)>;
	string_view write(const mutable_buffer &, const closure &);
	void read(const const_buffer &, const closure &);

	// Presents a secure buffer file IO closure for writing to path
	using mb_closure = std::function<string_view (const mutable_buffer &)>;
	void write_file(const string_view &path, const mb_closure &closure, const size_t &bufsz = 64_KiB);

	// Presents a secure buffer file IO closure with data read from path
	using cb_closure = std::function<void (const string_view &)>;
	void read_file(const string_view &path, const cb_closure &closure);
}

// OpenSSL BIGNUM convenience utils and wraps.
namespace ircd::openssl
{
	size_t size(const BIGNUM *const &); // bytes binary
	mutable_buffer data(const mutable_buffer &out, const BIGNUM *const &); // le
	string_view u2a(const mutable_buffer &out, const BIGNUM *const &);
}

/// Light semantic-complete wrapper for BIGNUM.
class ircd::openssl::bignum
{
	BIGNUM *a;

  public:
	const BIGNUM *get() const;
	BIGNUM *get();
	BIGNUM *release();

	size_t bits() const;
	size_t bytes() const;

	explicit operator uint128_t() const;
	operator const BIGNUM *() const;
	operator const BIGNUM &() const;
	operator BIGNUM *const &();
	operator BIGNUM **();
	operator BIGNUM &();

	// default constructor does not BN_new()
	bignum()
	:a{nullptr}
	{}

	// acquisitional constructor for OpenSSL API return values
	explicit bignum(BIGNUM *const &a)
	:a{a}
	{}

	explicit bignum(const uint128_t &val);
	bignum(const const_buffer &bin); // le
	explicit bignum(const BIGNUM &a);
	bignum(const bignum &);
	bignum(bignum &&) noexcept;
	bignum &operator=(const bignum &);
	bignum &operator=(bignum &&) noexcept;
	~bignum() noexcept;
};

struct ircd::openssl::init
{
	init();
	~init() noexcept;
};
