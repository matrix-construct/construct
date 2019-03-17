// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

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

size_t
ircd::buffer::zero(const mutable_buffer &buf)
{
	sodium_memzero(data(buf), size(buf));
	return size(buf);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/ed25519
//

static_assert(ircd::ed25519::SK_SZ == crypto_sign_ed25519_SECRETKEYBYTES);
static_assert(ircd::ed25519::PK_SZ == crypto_sign_ed25519_PUBLICKEYBYTES);

ircd::ed25519::sk::sk(pk *const &pk_arg,
                      const const_buffer &seed)
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

	const auto pk_data
	{
		reinterpret_cast<uint8_t *>(pk.data())
	};

	const auto seed_data
	{
		reinterpret_cast<const uint8_t *>(data(seed))
	};

	throw_on_error
	{
		::crypto_sign_ed25519_seed_keypair(pk_data, key.get(), seed_data)
	};
}

ircd::ed25519::sk::sk(const std::string &filename,
                      pk *const &pk_arg)
try
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

	const auto pk_data
	{
		reinterpret_cast<uint8_t *>(pk.data())
	};

	const mutable_buffer key_data
	{
		reinterpret_cast<char *>(key.get()), SK_SZ
	};

	if(!fs::exists(filename))
	{
		throw_on_error
		{
			::crypto_sign_ed25519_keypair(pk_data, key.get())
		};

		fs::write(filename, key_data);
	}
	else fs::read(filename, key_data);

	throw_on_error
	{
		::crypto_sign_ed25519_sk_to_pk(pk_data, key.get())
	};
}
catch(const std::exception &e)
{
	throw error
	{
		"Failed to read existing ed25519 secret key in: %s :%s",
		filename,
		e.what()
	};
}

ircd::ed25519::sig
ircd::ed25519::sk::sign(const const_buffer &msg)
const
{
	struct sig sig;
	unsigned long long sig_sz;

	const auto sig_data
	{
		reinterpret_cast<uint8_t *>(sig.data())
	};

	const auto msg_data
	{
		reinterpret_cast<const uint8_t *>(buffer::data(msg))
	};

	throw_on_error
	{
		::crypto_sign_ed25519_detached(sig_data,
		                               &sig_sz,
		                               msg_data,
		                               buffer::size(msg),
		                               key.get())
	};

	assert(sig_sz <= sig.size());
	assert(sig.size() >= sig_sz);
	memset(sig.data() + sig.size() - sig_sz, 0, sig.size() - sig_sz);
	return sig;
}

bool
ircd::ed25519::pk::verify(const const_buffer &msg,
                          const sig &sig)
const
{
	const auto sig_data
	{
		reinterpret_cast<const uint8_t *>(sig.data())
	};

	const auto msg_data
	{
		reinterpret_cast<const uint8_t *>(buffer::data(msg))
	};

	const auto key_data
	{
		reinterpret_cast<const uint8_t *>(data())
	};

	const int ret
	{
		::crypto_sign_ed25519_verify_detached(sig_data,
		                                      msg_data,
		                                      buffer::size(msg),
		                                      key_data)
	};

	return ret == 0?   true:
	       ret == -1?  false:
	                   throw nacl::error("verify failed: %d", ret);
}
