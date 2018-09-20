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
#include "compactor.h"
#include "prefix.h"
#include "merge.h"
#include "descriptor.h"
#include "database/rocksdb.h"
#include "database/database.h"
#include "database/options.h"
#include "database/snapshot.h"
#include "database/fileinfo.h"
#include "cache.h"
#include "opts.h"
#include "column.h"
#include "cell.h"
#include "row.h"
#include "index.h"
#include "json.h"
#include "txn.h"
#include "stats.h"

//
// Misc utils
//
namespace ircd::db
{
	extern const char *const version;

	// Utils for "name:checkpoint" string amalgam
	std::string namepoint(const string_view &name, const uint64_t &checkpoint);
	std::pair<string_view, uint64_t> namepoint(const string_view &name);

	// Generate local filesytem path based on name / name:checkpoint / etc.
	std::string path(const string_view &name, const uint64_t &checkpoint);
	std::string path(const string_view &name);

	std::vector<std::string> available();

	string_view reflect(const pos &);
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
