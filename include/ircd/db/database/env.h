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
#define HAVE_IRCD_DB_DATABASE_ENV_H

// This file is not part of the standard include stack because it requires
// RocksDB symbols which we cannot forward declare. It is used internally
// and does not need to be included by general users of IRCd.

#include <ircd/db/database/env/env.h>
#include <ircd/db/database/env/writable_file.h>
#include <ircd/db/database/env/sequential_file.h>
#include <ircd/db/database/env/random_access_file.h>
#include <ircd/db/database/env/random_rw_file.h>
#include <ircd/db/database/env/directory.h>
#include <ircd/db/database/env/file_lock.h>
#include <ircd/db/database/env/state.h>
