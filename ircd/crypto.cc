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

#include <openssl/sha.h>

namespace ircd::crh
{
	struct throw_on_error;

	static void finalize(struct sha256::ctx *const &, const mutable_raw_buffer &);
}

struct ircd::crh::throw_on_error
{
	throw_on_error(const int &openssl_return_value)
	{
		if(unlikely(openssl_return_value != 1))
			throw error("OpenSSL error code: %d", openssl_return_value);
	}
};

/// vtable
ircd::crh::hash::~hash()
noexcept
{
}

ircd::crh::hash &
ircd::crh::hash::operator+=(const const_buffer &buf)
{
	update(buf);
	return *this;
}

void
ircd::crh::hash::operator()(const mutable_raw_buffer &out,
                            const const_buffer &in)
{
	update(in);
	finalize(out);
}

void
ircd::crh::hash::finalize(const mutable_raw_buffer &buf)
const
{
	extract(buf);
}

struct ircd::crh::sha256::ctx
:SHA256_CTX
{
	ctx();
	~ctx() noexcept;
};

ircd::crh::sha256::ctx::ctx()
{
	throw_on_error
	{
		SHA256_Init(this)
	};
}

ircd::crh::sha256::ctx::~ctx()
noexcept
{
}

ircd::crh::sha256::sha256()
:ctx{std::make_unique<struct ctx>()}
{
}

/// One-shot functor. Immediately calls operator().
ircd::crh::sha256::sha256(const mutable_raw_buffer &out,
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
	throw_on_error
	{
		SHA256_Update(ctx.get(), data(buf), size(buf))
	};
}

void
ircd::crh::sha256::extract(const mutable_raw_buffer &buf)
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
	return bytes;
}

void
ircd::crh::finalize(struct sha256::ctx *const &ctx,
                    const mutable_raw_buffer &buf)
{
	uint8_t *const md
	{
		reinterpret_cast<uint8_t *>(data(buf))
	};

	throw_on_error
	{
		SHA256_Final(md, ctx)
	};
}
