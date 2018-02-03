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
#define HAVE_IRCD_ED25519_H

namespace ircd::ed25519
{
	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, bad_sig)

	const size_t SK_SZ { 32 + 32 };
	const size_t PK_SZ { 32 };
	const size_t SIG_SZ { 64 };
	const size_t SEED_SZ { 32 };

	struct pk;
	struct sk;
	struct sig;
}

class ircd::ed25519::sk
{
	std::unique_ptr<uint8_t[], void (*)(void *)> key;

  public:
	sig sign(const const_buffer &msg) const;

	sk(const std::string &filename, pk *const & = nullptr);
	sk(pk *const &, const const_buffer &seed);
	sk(): key{nullptr, std::free} {}
};

struct ircd::ed25519::pk
:fixed_mutable_buffer<PK_SZ>
{
	bool verify(const const_buffer &msg, const sig &) const;

	using fixed_mutable_buffer<PK_SZ>::fixed_mutable_buffer;

	pk()
	:fixed_mutable_buffer<PK_SZ>{nullptr}
	{}
};

struct ircd::ed25519::sig
:fixed_mutable_buffer<SIG_SZ>
{
	using fixed_mutable_buffer<SIG_SZ>::fixed_mutable_buffer;

	sig()
	:fixed_mutable_buffer<SIG_SZ>{nullptr}
	{}
};
