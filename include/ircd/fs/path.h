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
#define HAVE_IRCD_FS_PATH_H

// Forward declarations for boost because it is not included here.
namespace boost::filesystem
{
	struct path;
}

namespace ircd::fs
{
	enum base :uint;
	struct basepath;

	extern const size_t NAME_MAX_LEN;
	extern const size_t PATH_MAX_LEN;

	filesystem::path _path(std::string);
	filesystem::path _path(const string_view &);
	filesystem::path _path(const vector_view<const string_view> &);
	filesystem::path _path(const vector_view<const std::string> &);

	string_view path(const base &) noexcept;
	std::string path(const base &, const string_view &);
	std::string path(const vector_view<const string_view> &);
	std::string path(const vector_view<const std::string> &);
	std::string path(const filesystem::path &);

	size_t name_max_len(const string_view &path);
	size_t path_max_len(const string_view &path);

	long pathconf(const string_view &path, const int &arg);

	string_view cwd(const mutable_buffer &buf);
	std::string cwd();
}

/// A compile-time installation base-path. We have several of these in an
/// internal array accessible with get(enum base) or make_path(enum base).
struct ircd::fs::basepath
{
	string_view name;
	string_view path;

	static const basepath &get(const base &) noexcept;
};

/// Index of default paths. Must be aligned with the internal array in fs.cc.
/// Note that even though the PREFIX is accessible here, custom installations
/// may use entirely different paths for other components; most installations
/// use the package-target name as a path component.
enum ircd::fs::base
:uint
{
	PREFIX,     ///< Installation prefix (from ./configure --prefix)
	BIN,        ///< Binary directory (e.g. $prefix/bin)
	CONF,       ///< Configuration directory (e.g. $prefix/etc)
	DATA,       ///< Read-only data directory (e.g. $prefix/share)
	DB,         ///< Database directory (e.g. $prefix/var/db)
	LOG,        ///< Logfile directory (e.g. $prefix/var/log)
	MODULES,    ///< Modules directory (e.g. $prefix/lib/modules)

	_NUM_
};
