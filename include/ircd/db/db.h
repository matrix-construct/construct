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

	// Version information from rocksdb headers (when building ircd).
	extern const uint version[3];
	extern const string_view version_str;

	// Version of the RocksDB shared library (when running ircd).
	extern const uint abi_version[3];
	extern const string_view abi_version_str;

	// Supported compressions (detected when running ircd)
	extern std::array<std::string, 16> compressions;
}

#include "pos.h"
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
#include "database/sst.h"
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
	// Utils for "name:checkpoint" string amalgam
	std::string namepoint(const string_view &name, const uint64_t &checkpoint);
	std::pair<string_view, uint64_t> namepoint(const string_view &name);

	// Generate local filesytem path based on name / name:checkpoint / etc.
	std::string path(const string_view &name, const uint64_t &checkpoint);
	std::string path(const string_view &name);

	// Paths of available databases.
	std::vector<std::string> available();
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
