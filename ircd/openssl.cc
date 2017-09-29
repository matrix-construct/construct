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

///////////////////////////////////////////////////////////////////////////////
//
// hash.h
//

namespace ircd::crh
{
	static void finalize(struct sha256::ctx *const &, const mutable_raw_buffer &);
}

template<class exception = ircd::error,
         int ERR_CODE = 0,
         class function,
         class... args>
int
call_openssl(function&& f, args&&... a)
{
	const int ret
	{
		f(std::forward<args>(a)...)
	};

	if(unlikely(ret == ERR_CODE))
	{
		const unsigned long code{ERR_get_error()};
		const auto &msg{ERR_reason_error_string(code)?: "UNKNOWN ERROR"};
		throw exception("OpenSSL #%lu: %s", code, msg);
	}

	return ret;
};

struct ircd::crh::sha256::ctx
:SHA256_CTX
{
	ctx();
	~ctx() noexcept;
};

ircd::crh::sha256::ctx::ctx()
{
	call_openssl(::SHA256_Init, this);
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
	call_openssl(::SHA256_Update, ctx.get(), data(buf), size(buf));
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

	call_openssl(::SHA256_Final, md, ctx);
}
