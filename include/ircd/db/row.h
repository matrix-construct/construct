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
#define HAVE_IRCD_DB_ROW_H

namespace ircd {
namespace db   {

struct cell
{
	explicit
	cell(database &,
	     column,
	     const string_view &key,
	     gopts opts = {});
};

struct row
{
	using key_type = column;
	using mapped_type = std::unique_ptr<rocksdb::Iterator>;
	using value_type = std::pair<key_type, mapped_type>;

	gopts opts;
	std::vector<value_type> its;

	template<class pos> friend void seek(row &, const pos &);
	friend void seek(row &, const string_view &key);

  public:
	auto begin() const                           { return std::begin(its);                         }
	auto end() const                             { return std::end(its);                           }
	auto begin()                                 { return std::begin(its);                         }
	auto end()                                   { return std::end(its);                           }

	string_view operator[](const string_view &column);

	row(database &, const string_view &key = {}, gopts = {});
	row() = default;
	row(row &&) noexcept;
	row &operator=(row &&) noexcept;
	~row() noexcept;
};

} // namespace db
} // namespace ircd
