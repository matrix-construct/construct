/*
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
 */

#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/evp.h>

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
}

///////////////////////////////////////////////////////////////////////////////
//
// openssl.h
//

//
// X509
//

namespace ircd::openssl
{
	void append(X509_NAME &name, const string_view &key, const string_view &val);
	void append(X509_NAME &name, const json::object &entries);
	void append_entries(X509 &cert, const json::object &opts);
}

X509 &
ircd::openssl::peer_cert(SSL &ssl)
{
	auto *const ret
	{
		SSL_get_peer_certificate(&ssl)
	};

	assert(ret);
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
	return *ret;
}

ircd::string_view
ircd::openssl::genX509(const mutable_buffer &out,
                       const json::object &opts)
{
	const custom_ptr<RSA> rsa
	{
		RSA_new(),
		[](RSA *const rsa) { RSA_free(rsa); }
	};

	const custom_ptr<EVP_PKEY> pk
	{
		EVP_PKEY_new(),
		[](EVP_PKEY *const pk) { EVP_PKEY_free(pk); }
	};

	set(*pk, *rsa);

	const auto public_key_path
	{
		unquote(opts.at("tls_public_key_path"))
	};

	bio::read_file(public_key_path, [&pk]
	(const string_view &pem)
	{
		read_pem_pub(*pk, pem);
	});

	const auto private_key_path
	{
		unquote(opts.at("tls_private_key_path"))
	};

	bio::read_file(private_key_path, [&pk]
	(const string_view &pem)
	{
		read_pem_priv(*pk, pem);
	});

	check(*pk->pkey.rsa);

	const custom_ptr<X509> x509
	{
		X509_new(),
		[](X509 *const x509) { X509_free(x509); }
	};

	call(::X509_set_pubkey, x509.get(), pk.get());
	call(::X509_set_version, x509.get(), 2);
	append_entries(*x509, opts);

	call(::X509_sign, x509.get(), pk.get(), EVP_sha256());

	return write_pem(out, *x509);
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
			60 * 60 * 24 * opts.get<long>("days", 7L)
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

ircd::string_view
ircd::openssl::print(const mutable_buffer &buf,
                     const X509 &cert)
{
	return bio::write(buf, [&cert]
	(BIO *const &bio)
	{
		X509_print(bio, const_cast<X509 *>(&cert));
	});
}

ircd::const_raw_buffer
ircd::openssl::cert2d(const mutable_raw_buffer &out,
                      const string_view &pem)
{
	const custom_ptr<X509> x509
	{
		X509_new(),
		[](X509 *const x509) { X509_free(x509); }
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

ircd::const_raw_buffer
ircd::openssl::i2d(const mutable_raw_buffer &buf,
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

	uint8_t *out(data(buf));
	const const_raw_buffer ret
	{
		data(buf), size_t(i2d_X509(&cert, &out))
	};

	if(unlikely(size(ret) != size_t(len)))
		throw error();

	assert(out - data(buf) == len);
	return ret;
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
		RSA_new(),
		[](RSA *const rsa) { RSA_free(rsa); }
	};

	const custom_ptr<EVP_PKEY> pk
	{
		EVP_PKEY_new(),
		[](EVP_PKEY *const pk) { EVP_PKEY_free(pk); }
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

namespace ircd::openssl
{
	static int genrsa_cb(const int, const int, BN_GENCB *const);
}

RSA &
ircd::openssl::genrsa(RSA &out,
                      const uint &bits,
                      const uint &exp)
{
	BN_GENCB gencb{0};
	void *const arg{nullptr}; // privdata passed to cb
	BN_GENCB_set(&gencb, &ircd::openssl::genrsa_cb, arg);

	bignum e{exp};
	call(::RSA_generate_key_ex, &out, bits, e, &gencb);

	return out;
}

// This callback can be used to integrate generating with ircd::ctx
// or ctx::offload/thread or some status update. For now we just eat
// the milliseconds of prime generation on main.
// return false causes call(RSA_generate_key_ex) to throw
int
ircd::openssl::genrsa_cb(const int stat,
                         const int ith,
                         BN_GENCB *const ctx)
{
	assert(ctx != nullptr);
	auto &arg{ctx->arg};
	switch(stat)
	{
		case 0: // generating i-th potential prime
			return true;

		case 1: // testing i-th potential prime
			return true;

		case 2: // found i-th potential prime but rejected for RSA
			return true;

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

void
ircd::openssl::set(EVP_PKEY &out,
                   RSA &in)
{
	call(::EVP_PKEY_set1_RSA, &out, &in);
}

size_t
ircd::openssl::size(const RSA &key)
{
	assert(key.n != nullptr);
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
	return bio::write(out, [&p, &enc, &kstr, &klen, &pwcb, &u]
	(BIO *const &bio)
	{
		switch(p->type)
		{
			case EVP_PKEY_RSA:
				call(::PEM_write_bio_RSAPrivateKey, bio, p->pkey.rsa, enc, kstr, klen, pwcb, u);
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
	return bio::write(out, [&p]
	(BIO *const &bio)
	{
		switch(p->type)
		{
			case EVP_PKEY_RSA:
				call(::PEM_write_bio_RSAPublicKey, bio, p->pkey.rsa);
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
		switch(out->type)
		{
			case EVP_PKEY_RSA:
				ret = PEM_read_bio_RSAPrivateKey(bio, &out->pkey.rsa, pwcb, u);
				break;

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
		switch(out->type)
		{
			case EVP_PKEY_RSA:
				ret = PEM_read_bio_RSAPublicKey(bio, &out->pkey.rsa, pwcb, u);
				break;

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

ircd::string_view
ircd::openssl::version()
{
	return SSLeay_version(SSLEAY_VERSION);
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

	const custom_ptr<void> buf
	{
		OPENSSL_malloc_locked(size),
		[&size](void *const buf)
		{
			OPENSSL_cleanse(buf, size);
			OPENSSL_free_locked(buf);
		}
	};

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
	const custom_ptr<void> buf
	{
		OPENSSL_malloc_locked(size),
		[&size](void *const buf)
		{
			OPENSSL_cleanse(buf, size);
			OPENSSL_free_locked(buf);
		}
	};

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
		BIO_new_mem_buf(data(buf), size(buf)),
		[](BIO *const bp) { BIO_free(bp); }
	};

	closure(bp.get());
}

ircd::string_view
ircd::openssl::bio::write(const mutable_buffer &buf,
                          const closure &closure)
{
	const custom_ptr<BIO> bp
	{
		BIO_new(BIO_s_mem()),
		[](BIO *const bp) { BIO_free(bp); }
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
	const unique_buffer<mutable_raw_buffer> tmp
	{
		size(a)
	};

	return ircd::u2a(out, data(tmp, a));
}

ircd::mutable_raw_buffer
ircd::openssl::data(const mutable_raw_buffer &out,
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
		BN_bn2bin(a, data(out))
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
	const_raw_buffer
	{
		reinterpret_cast<const uint8_t *>(&val), sizeof(val)
	}
}
{
}

ircd::openssl::bignum::bignum(const const_raw_buffer &bin)
:a{[&bin]
{
	// Our binary buffer is little endian. We use
	const ctx::critical_assertion ca;
	static thread_local uint8_t tmp[64_KiB];
	const mutable_raw_buffer buf{tmp, size(bin)};
	if(unlikely(size(buf) > sizeof(tmp)))
		throw buffer_error
		{
			"buffer input of %zu for bignum > tmp %zu", size(bin), sizeof(tmp)
		};

	reverse(buf, bin);
	return BN_bin2bn(data(buf), size(buf), nullptr);
}()}
{
	if(unlikely(!a))
		throw error{"Error creating bignum from binary buffer..."};
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
	const mutable_raw_buffer buf
	{
		reinterpret_cast<uint8_t *>(&ret), sizeof(ret)
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
	OPENSSL_init();
	ERR_load_crypto_strings();
	ERR_load_ERR_strings();

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
{
	//assert(CRYPTO_get_locking_callback() == locking::callback);
	//assert(CRYPTO_THREADID_get_callback() == locking::id_callback);

	ERR_free_strings();
}

///////////////////////////////////////////////////////////////////////////////
//
// hash.h
//

namespace ircd::crh
{
	static void finalize(struct sha256::ctx *const &, const mutable_raw_buffer &);
}

struct ircd::crh::sha256::ctx
:SHA256_CTX
{
	ctx();
	~ctx() noexcept;
};

ircd::crh::sha256::ctx::ctx()
{
	openssl::call(::SHA256_Init, this);
}

ircd::crh::sha256::ctx::~ctx()
noexcept
{
}

ircd::crh::sha256::sha256()
:ctx{std::make_unique<struct ctx>()}
{
}

/// One-shot functor. Immediately calls update(); no output
ircd::crh::sha256::sha256(const const_raw_buffer &in)
:sha256{}
{
	update(in);
}

/// One-shot functor. Immediately calls operator().
ircd::crh::sha256::sha256(const mutable_raw_buffer &out,
                          const const_raw_buffer &in)
:sha256{}
{
	operator()(out, in);
}

ircd::crh::sha256::~sha256()
noexcept
{
}

void
ircd::crh::sha256::update(const const_raw_buffer &buf)
{
	openssl::call(::SHA256_Update, ctx.get(), data(buf), size(buf));
}

void
ircd::crh::sha256::digest(const mutable_raw_buffer &buf)
const
{
	auto copy(*ctx);
	crh::finalize(&copy, buf);
}

void
ircd::crh::sha256::finalize(const mutable_raw_buffer &buf)
{
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
                    const mutable_raw_buffer &buf)
{
	uint8_t *const md
	{
		reinterpret_cast<uint8_t *>(data(buf))
	};

	openssl::call(::SHA256_Final, md, ctx);
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

	std::shared_mutex mutex[CRYPTO_NUM_LOCKS];

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
