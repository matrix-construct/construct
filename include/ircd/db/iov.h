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
 *
 */

#pragma once
#define HAVE_IRCD_DB_IOV_H

namespace ircd::db
{
	struct iov;

	bool until(const iov &, const std::function<bool (const delta &)> &);
	void for_each(const iov &, const std::function<void (const delta &)> &);
	std::string debug(const iov &);
}

class ircd::db::iov
{
	database *d {nullptr};
	std::unique_ptr<rocksdb::WriteBatch> wb;

  public:
	struct opts;
	struct checkpoint;
	struct append;
	struct handler;

	explicit operator const rocksdb::WriteBatch &() const;
	explicit operator const database &() const;
	explicit operator rocksdb::WriteBatch &();
	explicit operator database &();

	string_view get(const op &, const string_view &col, const string_view &key) const;
	string_view at(const op &, const string_view &col, const string_view &key) const;
	bool has(const op &, const string_view &col, const string_view &key) const;

	delta get(const op &, const string_view &col) const;
	delta at(const op &, const string_view &col) const;
	bool has(const op &, const string_view &col) const;
	bool has(const op &) const;

	size_t bytes() const;
	size_t size() const;

	// commit
	void operator()(database &, const sopts & = {});
	void operator()(const sopts & = {});

	// clear
	void clear();

	iov() = default;
	iov(database &);
	iov(database &, const opts &);
	~iov() noexcept;
};

struct ircd::db::iov::append
{
	append(iov &, database &, const delta &);
	append(iov &, column &, const column::delta &);
	append(iov &, const cell::delta &);
	append(iov &, const row::delta &);
	append(iov &, const delta &);
	append(iov &, const string_view &key, const json::iov &);
};

struct ircd::db::iov::checkpoint
{
	iov &t;

	checkpoint(iov &);
	~checkpoint() noexcept;
};

struct ircd::db::iov::opts
{
	size_t reserve_bytes = 0;
	size_t max_bytes = 0;
};
