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
#define HAVE_IRCD_OPENSSL_H

// Forward declarations for OpenSSL because it is not included here. Note
// that these are declared in the extern namespace outside of ircd:: but
// match those in the OpenSSL headers and should not be too much trouble.
struct ssl_st;
struct rsa_st;
struct x509_st;
struct x509_store_ctx_st;
struct bignum_st;
struct bignum_ctx;
struct bio_st;
struct evp_pkey_st;

/// OpenSSL library interface. Provides things we need to expose from OpenSSL
/// to the rest of the project. Anything that employs forward declared types
/// here is lower level meant for definition files which may or may not include
/// OpenSSL directly but use this interface for DRY nonetheless. Otherwise
/// higher level wrappers should be used if available here.
namespace ircd::openssl
{
	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, buffer_error)

	struct init;
	struct bignum;

	using SSL = ::ssl_st;
	using RSA = ::rsa_st;
	using X509 = ::x509_st;
	using X509_STORE_CTX = ::x509_store_ctx_st;
	using BIGNUM = ::bignum_st;
	using BN_CTX = ::bignum_ctx;
	using EVP_PKEY = ::evp_pkey_st;
	using BIO = ::bio_st;

	// Library general
	string_view version();

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

	// RSA suite
	void check(const RSA &);
	bool check(const RSA &, std::nothrow_t);
	size_t size(const RSA &); // RSA_size() / mod size in bytes
	string_view print(const mutable_buffer &buf, const RSA &, const off_t &offset = 0);
	RSA &genrsa(RSA &out, const uint &bits = 2048, const uint &e = 0x10001);
	void genrsa(const string_view &skfile, const string_view &pkfile, const json::object &opts = {});
	void set(EVP_PKEY &out, RSA &in);

	// X.509 suite
	const_raw_buffer i2d(const mutable_raw_buffer &out, const X509 &);
	const_raw_buffer cert2d(const mutable_raw_buffer &out, const string_view &pem);
	X509 &read_pem(X509 &out, const string_view &pem);
	string_view write_pem(const mutable_buffer &out, const X509 &);
	string_view print(const mutable_buffer &buf, const X509 &, ulong flags = -1);
	string_view subject_common_name(const mutable_buffer &out, const X509 &);
	string_view genX509(const mutable_buffer &out, const json::object &opts);
	const X509 &peer_cert(const SSL &);
	X509 &peer_cert(SSL &);

	int get_error(const X509_STORE_CTX &);
	const char *cert_error_string(const long &);
	const char *get_error_string(const X509_STORE_CTX &);
	uint get_error_depth(const X509_STORE_CTX &);
	const X509 &current_cert(const X509_STORE_CTX &);
	X509 &current_cert(X509_STORE_CTX &);
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
	mutable_raw_buffer data(const mutable_raw_buffer &out, const BIGNUM *const &); // le
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
	bignum(const const_raw_buffer &bin); // le
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
