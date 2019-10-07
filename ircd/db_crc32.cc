// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#if __has_include("table/block_fetcher.h")
	#define ROCKSDB_PLATFORM_POSIX
	#define ZSTD_VERSION_NUMBER 0
	#include "table/block_fetcher.h"
	//#define IRCD_DB_BYPASS_CHECKSUM
#else
	#warning "The RocksDB source code is not available. Cannot interpose bugfixes for db/.h."
#endif

#if defined(IRCD_DB_BYPASS_CHECKSUM)
void
rocksdb::BlockFetcher::CheckBlockChecksum()
{
	//assert(0);
}
#endif
