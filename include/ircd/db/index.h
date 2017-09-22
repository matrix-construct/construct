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
#define HAVE_IRCD_DB_INDEX_H

namespace ircd::db
{
	struct index;
}

struct ircd::db::index
:column
{
	struct const_iterator_base;
	struct const_iterator;
	struct const_reverse_iterator;

	using iterator_type = const_iterator;
	using const_iterator_type = const_iterator;

	static const gopts applied_opts;

	const_iterator end(const string_view &key, gopts = {});
	const_iterator begin(const string_view &key, gopts = {});
	const_reverse_iterator rend(const string_view &key, gopts = {});
	const_reverse_iterator rbegin(const string_view &key, gopts = {});

	using column::column;
};

struct ircd::db::index::const_iterator_base
:ircd::db::column::const_iterator_base
{
	friend class index;

	const value_type &operator*() const;
	const value_type *operator->() const;

	using column::const_iterator_base::const_iterator_base;

	template<class pos> friend bool seek(index::const_iterator_base &, const pos &, gopts = {});
};

struct ircd::db::index::const_iterator
:ircd::db::index::const_iterator_base
{
	friend class index;

	const_iterator &operator++();
	const_iterator &operator--();

	using index::const_iterator_base::const_iterator_base;
};

struct ircd::db::index::const_reverse_iterator
:ircd::db::index::const_iterator_base
{
	friend class index;

	const_reverse_iterator &operator++();
	const_reverse_iterator &operator--();

	using index::const_iterator_base::const_iterator_base;
};
