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
#define HAVE_IRCD_ED25519_H

/// y^2 = x^3 + 486662x^2 + x  GF(2^255 − 19)
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
