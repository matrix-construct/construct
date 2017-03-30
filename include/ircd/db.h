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
#define HAVE_IRCD_DB_H

// IRCd Database
//
// Please see db/README.md for documentation.
//
namespace ircd {
namespace db   {

// Errors for the database subsystem. The exceptions that use _HIDENAME
// are built from RocksDB errors which already have an info string with
// an included name.
//
IRCD_EXCEPTION(ircd::error, error)
IRCD_EXCEPTION(error, not_found)
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

} // namespace db
} // namespace ircd

// These are forward declarations to objects we may carry a pointer to.
// Users of ircd::db should not be dealing with these types.
namespace rocksdb
{
	struct DB;
	struct Options;
	struct DBOptions;
	struct ColumnFamilyOptions;
	struct PlainTableOptions;
	struct BlockBasedTableOptions;
	struct Cache;
	struct Iterator;
	struct ColumnFamilyHandle;
	struct Snapshot;
}

#include "db/opts.h"
#include "db/delta.h"
#include "db/database.h"
#include "db/column.h"
#include "db/const_iterator.h"
#include "db/row.h"

namespace ircd {
namespace db   {

std::string path(const std::string &name);
std::vector<std::string> available();

} // namespace db
} // namespace ircd

namespace ircd {
namespace db   {

std::string merge_operator(const string_view &, const std::pair<string_view, string_view> &);

} // namespace db
} // namespace ircd
