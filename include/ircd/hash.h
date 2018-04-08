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
#define HAVE_IRCD_HASH_H

namespace ircd
{
	// constexpr bernstein string hasher suite; these functions will hash the
	// string at compile time leaving an integer residue at runtime. Decent
	// primes are at least 7681 and 5381.
	template<size_t PRIME = 7681> constexpr size_t hash(const char *const &str, const size_t i = 0);
	template<size_t PRIME = 7681> constexpr size_t hash(const char16_t *const &str, const size_t i = 0);
	template<size_t PRIME = 7681> constexpr size_t hash(const std::string_view &str, const size_t i = 0);

	// Note that at runtime this hash uses multiplication on every character
	// which can consume many cycles...
	template<size_t PRIME = 7681> size_t hash(const std::string &str, const size_t i = 0);
	template<size_t PRIME = 7681> size_t hash(const std::u16string &str, const size_t i = 0);
}

/// Collision-Resistant Hashing
///
/// ircd::crh contains support for collision-resistant hash functions including
/// cryptographic hash functions.
namespace ircd::crh
{
	IRCD_EXCEPTION(ircd::error, error)

	struct hash;
	struct sha256;
	struct ripemd160;
}

// Export aliases down to ircd::
namespace ircd
{
	using crh::hash;
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

/// SHA-256 hashing device.
struct ircd::crh::sha256
final
:hash
{
	struct ctx;

	static constexpr const size_t digest_size
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

	static constexpr const size_t digest_size
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

/// Runtime hashing of a std::u16string (for js). Non-cryptographic.
template<size_t PRIME>
size_t
ircd::hash(const std::u16string &str,
           const size_t i)
{
	return i >= str.size()? PRIME : (hash(str, i+1) * 33ULL) ^ str.at(i);
}

/// Runtime hashing of a std::string. Non-cryptographic.
template<size_t PRIME>
size_t
ircd::hash(const std::string &str,
           const size_t i)
{
	return i >= str.size()? PRIME : (hash(str, i+1) * 33ULL) ^ str.at(i);
}

/// Runtime hashing of a string_view. Non-cryptographic.
template<size_t PRIME>
constexpr size_t
ircd::hash(const std::string_view &str,
           const size_t i)
{
	return i >= str.size()? PRIME : (hash(str, i+1) * 33ULL) ^ str.at(i);
}

/// Compile-time hashing of a wider string literal (for js). Non-cryptographic.
template<size_t PRIME>
constexpr size_t
ircd::hash(const char16_t *const &str,
           const size_t i)
{
	return !str[i]? PRIME : (hash(str, i+1) * 33ULL) ^ str[i];
}

/// Compile-time hashing of a string literal. Non-cryptographic.
template<size_t PRIME>
constexpr size_t
ircd::hash(const char *const &str,
           const size_t i)
{
	return !str[i]? PRIME : (hash(str, i+1) * 33ULL) ^ str[i];
}
