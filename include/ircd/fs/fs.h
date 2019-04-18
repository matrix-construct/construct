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
#define HAVE_IRCD_FS_H

// Forward declarations for boost because it is not included here.
namespace boost::filesystem {}

/// Local filesystem interface.
///
/// IRCd has wrapped operations for the local filesystem to maintain a
/// cross-platform implementation of asynchronous file IO in conjunction with
/// the ircd::ctx userspace context system. These operations will yield your
/// ircd::ctx when necessary to not block the event loop on the main thread
/// during IOs.
///
/// Paths are stored in the platform-specific format using plain old character
/// strings, which means you should never directly manipulate path strings to
/// maintain x-platformness; instead use the (or add more) tools provided by
/// this interface (see: path.h).
///
namespace ircd::fs
{
	struct init;

	// Forward interface to boost::filesystem. We do not include boost
	// from here; it is used internally only. Some exposed interfaces
	// may make forward-declared references to boost symbols.
	namespace filesystem = boost::filesystem;

	// Runtime-detected support lights.
	extern const bool support_pwritev2;
	extern const bool support_append;
	extern const bool support_nowait;
	extern const bool support_hipri;
	extern const bool support_sync;
	extern const bool support_dsync;

	// Log facility for ircd::fs
	extern log::log log;
}

#include "error.h"
#include "path.h"
#include "iov.h"
#include "op.h"
#include "opts.h"
#include "dev.h"
#include "fd.h"
#include "wait.h"
#include "read.h"
#include "write.h"
#include "sync.h"
#include "aio.h"
#include "stdin.h"
#include "support.h"

namespace ircd::fs
{
	// Observers
	bool exists(const string_view &path);
	bool is_dir(const string_view &path);
	bool is_reg(const string_view &path);
	size_t size(const string_view &path);
	std::vector<std::string> ls(const string_view &path);
	std::vector<std::string> ls_r(const string_view &path);

	// Modifiers
	bool rename(std::nothrow_t, const string_view &old, const string_view &new_);
	bool rename(const string_view &old, const string_view &new_);
	bool remove(std::nothrow_t, const string_view &path);
	bool remove(const string_view &path);
	bool mkdir(const string_view &path);
}

/// Filesystem interface init / fini held by ircd::main().
struct ircd::fs::init
{
	aio::init _aio_;

	init();
	~init() noexcept;
};
