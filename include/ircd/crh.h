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
#define HAVE_IRCD_CRH_H

/// Collision-Resistant Hashing
///
/// ircd::crh contains support for collision-resistant hash functions including
/// cryptographic hash functions.
namespace ircd::crh
{
	IRCD_EXCEPTION(ircd::error, error)

	struct hash;
	struct sha1;
	struct sha256;
	struct ripemd160;
}

// Export aliases down to ircd::
namespace ircd
{
	using crh::sha1;
	using crh::sha256;
	using crh::ripemd160;
}

/// Abstract interface to a hashing context for any algorithm in ircd::crh
///
/// Use this type when dealing with algorithm-agnostic hashing.
struct ircd::crh::hash
{
	/// Returns the byte length of the mutable_buffer for digests
	virtual size_t length() const = 0;

	/// Samples the digest at the current state (without modifying)
	virtual void digest(const mutable_buffer &) const = 0;

	/// Samples the digest and modifies the state (depending on impl)
	virtual void finalize(const mutable_buffer &b);

	/// Appends to the message
	virtual void update(const const_buffer &) = 0;

	// conveniences for output
	template<size_t SIZE> fixed_const_buffer<SIZE> digest() const;
	template<size_t SIZE> operator fixed_const_buffer<SIZE>() const;

	// conveniences for input
	void operator()(const mutable_buffer &out, const const_buffer &in);
	hash &operator+=(const const_buffer &);

	virtual ~hash() noexcept;
};

/// SHA-1 hashing device.
struct ircd::crh::sha1
final
:hash
{
	struct ctx;

	static constexpr const size_t &digest_size
	{
		160 / 8
	};

	using buf = fixed_const_buffer<digest_size>;

  protected:
	std::unique_ptr<ctx> ctx;

  public:
	size_t length() const override;
	void digest(const mutable_buffer &) const override;
	void finalize(const mutable_buffer &) override;
	void update(const const_buffer &) override;

	sha1(const mutable_buffer &, const const_buffer &); // note: finalizes
	sha1(const const_buffer &); // note: finalizes
	sha1();
	~sha1() noexcept;
};

/// SHA-256 hashing device.
struct ircd::crh::sha256
final
:hash
{
	struct ctx;

	static constexpr const size_t &digest_size
	{
		256 / 8
	};

	using buf = fixed_const_buffer<digest_size>;

  protected:
	std::unique_ptr<ctx> ctx;

  public:
	size_t length() const override;
	void digest(const mutable_buffer &) const override;
	void finalize(const mutable_buffer &) override;
	void update(const const_buffer &) override;

	sha256(const mutable_buffer &, const const_buffer &); // note: finalizes
	sha256(const const_buffer &); // note: finalizes
	sha256();
	~sha256() noexcept;
};

/// RIPEMD160 hashing device.
struct ircd::crh::ripemd160
final
:hash
{
	struct ctx;

	static constexpr const size_t &digest_size
	{
		160 / 8
	};

	using buf = fixed_const_buffer<digest_size>;

  protected:
	std::unique_ptr<ctx> ctx;

  public:
	size_t length() const override;
	void digest(const mutable_buffer &) const override;
	void finalize(const mutable_buffer &) override;
	void update(const const_buffer &) override;

	ripemd160(const mutable_buffer &, const const_buffer &); // note: finalizes
	ripemd160(const const_buffer &); // note: finalizes
	ripemd160();
	~ripemd160() noexcept;
};

/// Automatic gratification from hash::digest()
template<size_t SIZE>
ircd::crh::hash::operator
fixed_const_buffer<SIZE>()
const
{
	return digest<SIZE>();
}

/// Digests the hash into the buffer of the specified SIZE and returns it
template<size_t SIZE>
ircd::fixed_const_buffer<SIZE>
ircd::crh::hash::digest()
const
{
	assert(SIZE >= length());
	return fixed_const_buffer<SIZE>
	{
		[this](const auto &buffer)
		{
			this->digest(buffer);
		}
	};
}
