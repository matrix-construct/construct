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
#define HAVE_IRCD_HASH_H

namespace ircd
{
	// constexpr bernstein string hasher suite; these functions will hash the
	// string at compile time leaving an integer residue at runtime.
	constexpr size_t hash(const char *const &str, const size_t i = 0);
	constexpr size_t hash(const char16_t *const &str, const size_t i = 0);

	// Note that at runtime this hash uses multiplication on every character
	// which can consume many cycles...
	size_t hash(const std::string &str, const size_t i = 0);
	size_t hash(const std::u16string &str, const size_t i = 0);
	size_t hash(const std::string_view &str, const size_t i = 0);

	/// ircd reserves the $ character (in our namespace of course) as an alias
	/// for hashing string literals at compile time.
	template<class string>
	constexpr size_t $(string&& s)
	{
		return hash(std::forward<string>(s));
	}
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
}

// Export aliases down to ircd::
namespace ircd
{
	using crh::hash;
	using crh::sha256;
}

/// Abstract interface to a hashing context for any algorithm in ircd::crh
///
/// Use this type when dealing with algorithm-agnostic hashing.
struct ircd::crh::hash
{
	virtual size_t length() const = 0;
	virtual void finalize(const mutable_raw_buffer &) = 0;
	virtual void extract(const mutable_raw_buffer &) const = 0;
	virtual void update(const const_raw_buffer &) = 0;

	// conveniences
	void finalize(const mutable_raw_buffer &) const;
	void operator()(const mutable_raw_buffer &out, const const_raw_buffer &in);
	hash &operator+=(const const_raw_buffer &);

	virtual ~hash() noexcept;
};

struct ircd::crh::sha256
:hash
{
	struct ctx;

	static constexpr const size_t bytes
	{
		256 / 8
	};

  protected:
	std::unique_ptr<ctx> ctx;

  public:
	size_t length() const override;
	void finalize(const mutable_raw_buffer &) override;
	void extract(const mutable_raw_buffer &) const override;
	void update(const const_raw_buffer &) override;

	sha256(const mutable_raw_buffer &, const const_raw_buffer &);
	sha256();
	~sha256() noexcept;
};

inline size_t
ircd::hash(const std::string_view &str,
           const size_t i)
{
	return i >= str.size()? 7681ULL : (hash(str, i+1) * 33ULL) ^ str.at(i);
}

inline size_t
ircd::hash(const std::u16string &str,
           const size_t i)
{
	return i >= str.size()? 7681ULL : (hash(str, i+1) * 33ULL) ^ str.at(i);
}

inline size_t
ircd::hash(const std::string &str,
           const size_t i)
{
	return i >= str.size()? 7681ULL : (hash(str, i+1) * 33ULL) ^ str.at(i);
}

constexpr size_t
ircd::hash(const char16_t *const &str,
           const size_t i)
{
	return !str[i]? 7681ULL : (hash(str, i+1) * 33ULL) ^ str[i];
}

constexpr size_t
ircd::hash(const char *const &str,
           const size_t i)
{
	return !str[i]? 7681ULL : (hash(str, i+1) * 33ULL) ^ str[i];
}
