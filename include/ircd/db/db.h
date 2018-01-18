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
#define HAVE_IRCD_DB_H

/// Database: an object store from the primitives of `cell`, `column`, and `row`.
namespace ircd::db
{
	struct init;
	struct gopts;
	struct sopts;
	struct cell;
	struct row;
	struct column;
	struct index;
	struct database;
	enum class pos :int8_t;

	// Errors for the database subsystem. The exceptions that use _HIDENAME
	// are built from RocksDB errors which already have an info string with
	// an included name.
	//
	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, not_found)
	IRCD_EXCEPTION(error, schema_error)
	IRCD_EXCEPTION_HIDENAME(error, corruption)
	IRCD_EXCEPTION_HIDENAME(error, not_supported)
	IRCD_EXCEPTION_HIDENAME(error, invalid_argument)
	IRCD_EXCEPTION_HIDENAME(error, io_error)
	IRCD_EXCEPTION_HIDENAME(error, merge_in_progress)
	IRCD_EXCEPTION_HIDENAME(error, incomplete)
	IRCD_EXCEPTION_HIDENAME(error, shutdown_in_progress)
	IRCD_EXCEPTION_HIDENAME(error, timed_out)
	IRCD_EXCEPTION_HIDENAME(error, aborted)
	IRCD_EXCEPTION_HIDENAME(error, busy)
	IRCD_EXCEPTION_HIDENAME(error, expired)
	IRCD_EXCEPTION_HIDENAME(error, try_again)

	// db subsystem has its own logging facility
	extern struct log::log log;
}

/// Types of iterator operations
enum class ircd::db::pos
:int8_t
{
	FRONT   = -2,    // .front()    | first element
	PREV    = -1,    // std::prev() | previous element
	END     = 0,     // break;      | exit iteration (or past the end)
	NEXT    = 1,     // continue;   | next element
	BACK    = 2,     // .back()     | last element
};

#include "delta.h"
#include "comparator.h"
#include "prefix.h"
#include "merge.h"
#include "database.h"
#include "snapshot.h"
#include "opts.h"
#include "column.h"
#include "cell.h"
#include "row.h"
#include "index.h"
#include "json.h"
#include "iov.h"

//
// Misc utils
//
namespace ircd::db
{
	extern const char *const version;

	rocksdb::Slice slice(const string_view &);
	string_view slice(const rocksdb::Slice &);

	bool valid(const rocksdb::Iterator &);
	string_view key(const rocksdb::Iterator &);
	string_view val(const rocksdb::Iterator &);

	std::string path(const std::string &name);
	std::vector<std::string> available();

	void log_rdb_perf_context(const bool &all = true);

	string_view reflect(const pos &);

	// Indicates an op uses both a key and value for its operation. Some only use
	// a key name so an empty value argument in a delta is okay when false.
	bool value_required(const op &);
	string_view reflect(const op &);
}

namespace ircd
{
	using db::database;
}

/// Database subsystem initialization and destruction
struct ircd::db::init
{
	init();
	~init() noexcept;
};
