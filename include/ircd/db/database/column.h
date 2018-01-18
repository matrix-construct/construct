//
// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#pragma once
#define HAVE_IRCD_DB_DATABASE_COLUMN_H

// This file is not part of the standard include stack because it requires
// RocksDB symbols which we cannot forward declare. It is used internally
// and does not need to be included by general users of IRCd.

struct ircd::db::database::column final
:std::enable_shared_from_this<database::column>
,rocksdb::ColumnFamilyDescriptor
{
	database *d;
	std::type_index key_type;
	std::type_index mapped_type;
	database::descriptor descriptor;
	comparator cmp;
	prefix_transform prefix;
	custom_ptr<rocksdb::ColumnFamilyHandle> handle;

  public:
	operator const rocksdb::ColumnFamilyOptions &();
	operator const rocksdb::ColumnFamilyHandle *() const;
	operator const database &() const;

	operator rocksdb::ColumnFamilyOptions &();
	operator rocksdb::ColumnFamilyHandle *();
	operator database &();

	explicit column(database *const &d, const database::descriptor &);
	column() = delete;
	column(column &&) = delete;
	column(const column &) = delete;
	column &operator=(column &&) = delete;
	column &operator=(const column &) = delete;
	~column() noexcept;

	friend void flush(column &, const bool &blocking);
};
