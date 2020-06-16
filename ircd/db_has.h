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
