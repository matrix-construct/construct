// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#if ROCKSDB_MAJOR > 5 \
|| (ROCKSDB_MAJOR == 5 && ROCKSDB_MINOR >= 18)
	#define IRCD_DB_HAS_ALLOCATOR
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 1) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 1 && ROCKSDB_PATCH >= 1)
	#define IRCD_DB_HAS_ENV_PRIO_USER
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 1) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 1 && ROCKSDB_PATCH >= 1)
	#define IRCD_DB_HAS_SECONDARY
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 2) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 2 && ROCKSDB_PATCH >= 2)
	#define IRCD_DB_HAS_PERIODIC_COMPACTIONS
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 2) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 2 && ROCKSDB_PATCH >= 2)
	#define IRCD_DB_HAS_MULTIGET_SINGLE
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 3) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 3 && ROCKSDB_PATCH >= 6)
	#define IRCD_DB_HAS_ENV_MULTIREAD
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 3) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 3 && ROCKSDB_PATCH >= 6)
	#define IRCD_DB_HAS_CF_DROPPED
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 3) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 3 && ROCKSDB_PATCH >= 6)
	#define IRCD_DB_HAS_TIMESTAMP
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 4) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 4 && ROCKSDB_PATCH >= 6)
	#define IRCD_DB_HAS_CACHE_GETCHARGE
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 6) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 6 && ROCKSDB_PATCH >= 3)
	#define IRCD_DB_HAS_MULTIGET_BATCHED
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 7) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 7 && ROCKSDB_PATCH >= 3)
	#define IRCD_DB_HAS_ENV_FILESYSTEM
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 10) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 10 && ROCKSDB_PATCH >= 0)
	#define IRCD_DB_HAS_MULTIGET_DIRECT
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 12) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 12 && ROCKSDB_PATCH >= 6)
	#define IRCD_DB_HAS_MANIFEST_WRITE
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 14) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 14 && ROCKSDB_PATCH >= 5)
	#define IRCD_DB_HAS_FLUSH_RETRY
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 16) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 16 && ROCKSDB_PATCH >= 3)
	#define IRCD_DB_HAS_MANIFEST_WRITE_NOWAL
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 19) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 19 && ROCKSDB_PATCH >= 3)
	#define IRCD_DB_HAS_VERSION_ABI
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 20) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 20 && ROCKSDB_PATCH >= 3)
	#define IRCD_DB_HAS_WAL_FULL
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 22) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 22 && ROCKSDB_PATCH >= 1)
	#define IRCD_DB_HAS_CACHE_GETDELETER
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 22) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 22 && ROCKSDB_PATCH >= 1)
	#define IRCD_DB_HAS_CACHE_APPLYTOALL
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 24) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 24 && ROCKSDB_PATCH >= 2)
	#define IRCD_DB_HAS_CHANGE_TEMPERATURE
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 25) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 25 && ROCKSDB_PATCH >= 1)
	#define IRCD_DB_HAS_IO_MID
	#define IRCD_DB_HAS_IO_USER
#endif

#if ROCKSDB_MAJOR > 6 \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR > 26) \
|| (ROCKSDB_MAJOR == 6 && ROCKSDB_MINOR == 26 && ROCKSDB_PATCH >= 0)
	#define IRCD_DB_HAS_FORCED_BLOBGC
#endif
