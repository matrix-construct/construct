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
 */

#pragma once
#define HAVE_IRCD_CUCKOO_H

/// Cuckoo filtering and hashing.
///
/// This is an API for building cuckoo filters for efficient set membership
/// tests. Cuckoo filters are a recently celebrated result by Bin Fan inspired
/// by Mitzenmacher's seminal thesis: the Power Of Two choices in randomized
/// linear load balancing. These filters are used extensively to optimize
/// queries made into matrix room state after accumulating events.
///
/// Note that the hash residue has to be kept secret from an adversary who
/// may try to craft strings to attack the filter. A secret salt should be
/// used. Filters may also be serialized to the db so the salt will have to
/// persist secretly too.
///
namespace ircd::cuckoo
{
	IRCD_EXCEPTION(ircd::error, error)

	template<class word, size_t words> struct entry;
	template<class entry, size_t entries> struct bucket;
	template<class bucket, size_t buckets> struct table;

	struct filter;
	struct counter;
}

template<class word,
         size_t words>
struct ircd::cuckoo::entry
:std::array<word, words>
{
	using array_type = std::array<word, words>;

};

template<class entry,
         size_t entries>
struct ircd::cuckoo::bucket
:std::array<entry, entries>
{
	using array_type = std::array<entry, entries>;

};

template<class bucket,
         size_t buckets>
struct ircd::cuckoo::table
:std::array<bucket, buckets>
{
	using array_type = std::array<bucket, buckets>;

	static_assert(is_powerof2(buckets), "Bucket count must be power of 2");
};

struct ircd::cuckoo::filter
{
	using entry = cuckoo::entry<uint8_t, 1>;
	using bucket = cuckoo::bucket<entry, 1>;
	using table = cuckoo::table<bucket, 32>;

	table a, b;

	bool has(const string_view &) const;

	void add(const string_view &);
	void del(const string_view &);
};

struct ircd::cuckoo::counter
{
	using int_t = int32_t;
	using entry = cuckoo::entry<int_t, 1>;
	using bucket = cuckoo::bucket<entry, 1>;
	using table = cuckoo::table<bucket, 32>;

	table a, b;

	int_t count(const string_view &) const;
	bool has(const string_view &) const;

	void add(const string_view &);
	void del(const string_view &);
};
