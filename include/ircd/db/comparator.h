/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
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
#define HAVE_IRCD_DB_COMPARATOR_H

namespace ircd::db
{
	struct comparator;

	struct cmp_int64_t;
	struct cmp_string_view;
}

struct ircd::db::comparator
{
	string_view name;
	std::function<bool (const string_view &, const string_view &)> less;
	std::function<bool (const string_view &, const string_view &)> equal;
};

struct ircd::db::cmp_string_view
:db::comparator
{
	static bool less(const string_view &a, const string_view &b)
	{
		return a < b;
	}

	static bool equal(const string_view &a, const string_view &b)
	{
		return a == b;
	}

	cmp_string_view()
	:db::comparator{"string_view", less, equal}
	{}
};

struct ircd::db::cmp_int64_t
:db::comparator
{
	static bool less(const string_view &sa, const string_view &sb)
	{
		assert(sa.size() == sizeof(int64_t));
		assert(sb.size() == sizeof(int64_t));
		const byte_view<int64_t> a{sa};
		const byte_view<int64_t> b{sb};
		return a < b;
	}

	static bool equal(const string_view &sa, const string_view &sb)
	{
		assert(sa.size() == sizeof(int64_t));
		assert(sb.size() == sizeof(int64_t));
		const byte_view<int64_t> a{sa};
		const byte_view<int64_t> b{sb};
		return a == b;
	}

	cmp_int64_t()
	:db::comparator{"int64_t", less, equal}
	{}
};
