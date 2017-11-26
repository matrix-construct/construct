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
#include <openssl/x509.h>

namespace ircd::openssl
{
	template<class exception = openssl::error>
	static void throw_error(const ulong &);

	template<class exception = openssl::error>
	static void throw_error();

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

ircd::const_raw_buffer
ircd::openssl::cert2d(const mutable_raw_buffer &out,
                      const string_view &cert)
{
	X509 x509 {0};
	return i2d(out, read(&x509, cert));
}

X509 &
ircd::openssl::read(X509 *out,
                    const string_view &cert)
{
	const custom_ptr<BIO> bp
	{
		BIO_new_mem_buf(data(cert), size(cert)),
		[](BIO *const bp) { BIO_free(bp); }
	};

	X509 *const ret
	{
		PEM_read_bio_X509(bp.get(), &out, nullptr, nullptr)
	};

	if(unlikely(ret != out))
	{
		throw_error();
		__builtin_unreachable();
	}

	return *ret;
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
	{
		throw_error();
		__builtin_unreachable();
	}

	if(unlikely(size(buf) < size_t(len)))
		throw error
		{
			"DER requires a %zu byte buffer, you supplied %zu bytes", len, size(buf)
		};

	uint8_t *out(data(buf));
	const const_raw_buffer ret
	{
		data(buf),
		size_t(i2d_X509(&cert, &out))
	};

	if(unlikely(size(ret) != size_t(len)))
		throw error();

	assert(out - data(buf) == len);
	return ret;
}

X509 &
ircd::openssl::get_peer_cert(SSL &ssl)
{
	auto *const ret
	{
		SSL_get_peer_certificate(&ssl)
	};

	assert(ret);
	return *ret;
}

const X509 &
ircd::openssl::get_peer_cert(const SSL &ssl)
{
	const auto *const ret
	{
		SSL_get_peer_certificate(&ssl)
	};

	assert(ret);
	return *ret;
}

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
	const int ret
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
	const auto &msg{ERR_reason_error_string(code)?: "UNKNOWN ERROR"};
	throw exception("OpenSSL #%lu: %s", code, msg);
}

template<class exception>
static void
ircd::openssl::throw_error()
{
	const auto code{get_error()};
	throw_error(code);
	__builtin_unreachable();
}
