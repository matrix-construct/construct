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
 *
 */

#include <sodium.h>

///////////////////////////////////////////////////////////////////////////////
//
// Internal
//

struct throw_on_error
{
	throw_on_error(const int &val)
	{
		if(unlikely(val != 0))
			throw ircd::nacl::error("sodium error");
	}
};

///////////////////////////////////////////////////////////////////////////////
//
// ircd/nacl.h
//

ircd::string_view
ircd::nacl::version()
{
	return ::sodium_version_string();
}

ircd::nacl::init::init()
{
	if(::sodium_init() < 0)
		throw std::runtime_error("sodium_init(): error");
}

ircd::nacl::init::~init()
noexcept
{
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/buffer.h
//

void
ircd::buffer::zero(const mutable_raw_buffer &buf)
{
	sodium_memzero(data(buf), size(buf));
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/ed25519
//

static_assert(ircd::ed25519::SK_SZ == crypto_sign_ed25519_SECRETKEYBYTES);
static_assert(ircd::ed25519::PK_SZ == crypto_sign_ed25519_PUBLICKEYBYTES);

ircd::ed25519::sk::sk(pk *const &pk_arg,
                      const const_raw_buffer &seed)
:key
{
	reinterpret_cast<uint8_t *>(::sodium_malloc(crypto_sign_ed25519_SECRETKEYBYTES)),
	&::sodium_free
}
{
	assert(size(seed) >= SEED_SZ);

	pk discard, &pk
	{
		pk_arg? *pk_arg : discard
	};

	throw_on_error
	{
		::crypto_sign_ed25519_seed_keypair(pk.data(), key.get(), data(seed))
	};
}

ircd::ed25519::sk::sk(const std::string &filename,
                      pk *const &pk_arg)
:key
{
	reinterpret_cast<uint8_t *>(::sodium_malloc(crypto_sign_ed25519_SECRETKEYBYTES)),
	&::sodium_free
}
{
	pk discard, &pk
	{
		pk_arg? *pk_arg : discard
	};

	const auto existing
	{
		fs::read(filename, mutable_raw_buffer{key.get(), SK_SZ})
	};

	if(!existing)
	{
		if(fs::exists(filename))
			throw error("Failed to read existing ed25519 secret key in: %s", filename);

		throw_on_error
		{
			::crypto_sign_ed25519_keypair(pk.data(), key.get())
		};

		fs::write(filename, const_raw_buffer{key.get(), SK_SZ});
	}

	throw_on_error
	{
		::crypto_sign_ed25519_sk_to_pk(pk.data(), key.get())
	};
}

ircd::ed25519::sig
ircd::ed25519::sk::sign(const const_raw_buffer &msg)
const
{
	unsigned long long sig_sz;
	struct sig sig;
	throw_on_error
	{
		::crypto_sign_ed25519_detached(sig.data(),
		                               &sig_sz,
		                               buffer::data(msg),
		                               buffer::size(msg),
		                               key.get())
	};

	assert(sig_sz <= sig.size());
	assert(sig.size() >= sig_sz);
	memset(sig.data() + sig.size() - sig_sz, 0, sig.size() - sig_sz);
	return sig;
}

bool
ircd::ed25519::pk::verify(const const_raw_buffer &msg,
                          const sig &sig)
const
{
	const int ret
	{
		::crypto_sign_ed25519_verify_detached(sig.data(),
		                                      buffer::data(msg),
		                                      buffer::size(msg),
		                                      data())
	};

	return ret == 0?   true:
	       ret == -1?  false:
	                   throw nacl::error("verify failed: %d", ret);
}
