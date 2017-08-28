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
#define HAVE_IRCD_DB_OPTS_H

namespace ircd::db
{
	template<class T> struct optval;
	template<class T> using optlist = std::initializer_list<optval<T>>;
	template<class T> bool has_opt(const optlist<T> &, const T &);
	template<class T> ssize_t opt_val(const optlist<T> &, const T &);

	enum class set;
	struct sopts;

	enum class get;
	struct gopts;
}

template<class T>
struct ircd::db::optval
:std::pair<T, ssize_t>
{
	optval(const T &key, const ssize_t &val = std::numeric_limits<ssize_t>::min());
};

enum class ircd::db::set
{
	FSYNC,                  // Uses kernel filesystem synchronization after write (slow)
	NO_JOURNAL,             // Write Ahead Log (WAL) for some crash recovery
	MISSING_COLUMNS         // No exception thrown when writing to a deleted column family
};

struct ircd::db::sopts
:optlist<set>
{
	template<class... list>
	sopts(list&&... l)
	:optlist<set>{std::forward<list>(l)...}
	{}
};

enum class ircd::db::get
{
	PIN,                    // Keep iter data in memory for iter lifetime (good for lots of ++/--)
	CACHE,                  // Update the cache (CACHE is default for non-iterator operations)
	NO_CACHE,               // Do not update the cache (NO_CACHE is default for iterators)
	NO_SNAPSHOT,            // This iterator will have the latest data (tailing)
	NO_CHECKSUM,            // Integrity of data will be checked unless this is specified
	READAHEAD,              // Pair with a size in bytes for prefetching additional data
	NO_EMPTY,               // Option for db::row to not include unassigned cells in the row
};

struct ircd::db::gopts
:optlist<get>
{
	database::snapshot snapshot;

	template<class... list>
	gopts(list&&... l)
	:optlist<get>{std::forward<list>(l)...}
	{}
};

template<class T>
ssize_t
ircd::db::opt_val(const optlist<T> &list,
                  const T &opt)
{
	for(const auto &p : list)
		if(p.first == opt)
			return p.second;

	return std::numeric_limits<ssize_t>::min();
}

template<class T>
bool
ircd::db::has_opt(const optlist<T> &list,
                  const T &opt)
{
	for(const auto &p : list)
		if(p.first == opt)
			return true;

	return false;
}

template<class T>
ircd::db::optval<T>::optval(const T &key,
                            const ssize_t &val)
:std::pair<T, ssize_t>{key, val}
{
}
