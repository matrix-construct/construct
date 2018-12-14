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

/*
 * Directory paths and filenames for UNIX systems.
 * IRCD_PREFIX is set using ./configure --prefix, see INSTALL.
 * Do not change these without corresponding changes in the build system.
 *
 * IRCD_PREFIX = prefix for all directories,
 * DPATH       = root directory of installation,
 * BINPATH     = directory for binary files,
 * ETCPATH     = directory for configuration files,
 * LOGPATH     = directory for logfiles,
 * MODPATH     = directory for modules,
 * AUTOMODPATH = directory for autoloaded modules
 */

/// Tools for working with the local filesystem.
///
/// IRCd has wrapped operations for the local filesystem to maintain a
/// cross-platform implementation of asynchronous file IO in conjunction with
/// the ircd::ctx userspace context system. These operations will yield your
/// ircd::ctx when necessary to not block the event loop on the main thread
/// during IOs.
namespace ircd::fs
{
	struct init;
	enum index :int;
	struct error; // custom exception; still inherits from ircd::error

	constexpr size_t PATH_MAX { 2048 };

	string_view get(index) noexcept;
	string_view name(index) noexcept;
	std::string make_path(const vector_view<const string_view> &);
	std::string make_path(const vector_view<const std::string> &);

	bool exists(const string_view &path);
	bool is_dir(const string_view &path);
	bool is_reg(const string_view &path);
	size_t size(const string_view &path);
	bool direct_io_support(const string_view &path);

	std::vector<std::string> ls(const string_view &path);
	std::vector<std::string> ls_recursive(const string_view &path);

	bool rename(std::nothrow_t, const string_view &old, const string_view &new_);
	void rename(const string_view &old, const string_view &new_);
	bool remove(std::nothrow_t, const string_view &path);
	bool remove(const string_view &path);

	void chdir(const string_view &path);
	bool mkdir(const string_view &path);

	std::string cwd();
}

/// Index of default paths. Must be aligned with fs::syspaths (see: fs.cc)
enum ircd::fs::index
:int
{
	PREFIX,
	BIN,
	CONF,
	DATA,
	DB,
	LOG,
	MODULES,

	_NUM_
};

#include "error.h"
#include "iov.h"
#include "fd.h"
#include "aio.h"
#include "read.h"
#include "write.h"
#include "sync.h"
#include "stdin.h"
#include "support.h"

struct ircd::fs::init
{
	aio::init _aio_;

	init();
	~init() noexcept;
};
