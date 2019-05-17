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

	// convenience tls buffers of appropriate size.
	extern const mutable_buffer path_scratch;
	extern const mutable_buffer name_scratch;

	filesystem::path _path(std::string);
	filesystem::path _path(const string_view &);
	filesystem::path _path(const vector_view<const string_view> &);
	filesystem::path _path(const vector_view<const std::string> &);

	string_view path(const base &) noexcept;
	string_view path(const mutable_buffer &, const base &, const string_view &);
	string_view path(const mutable_buffer &, const vector_view<const string_view> &);
	string_view path(const mutable_buffer &, const vector_view<const std::string> &);
	string_view path(const mutable_buffer &, const filesystem::path &);
	template<class... A> std::string path_string(A&&...);

	bool is_relative(const string_view &path);
	bool is_absolute(const string_view &path);

	string_view extension(const mutable_buffer &, const string_view &path, const string_view &replace);
	string_view extension(const mutable_buffer &, const string_view &path);
	string_view relative(const mutable_buffer &, const string_view &root, const string_view &path);
	string_view filename(const mutable_buffer &, const string_view &path);
	string_view parent(const mutable_buffer &, const string_view &path);

	long pathconf(const string_view &path, const int &arg);
	size_t name_max_len(const string_view &path);
	size_t path_max_len(const string_view &path);

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

template<class... A>
std::string
ircd::fs::path_string(A&&... a)
{
	const size_t &size
	{
		PATH_MAX_LEN | SHRINK_TO_FIT
	};

	return util::string(size, [&a...]
	(const mutable_buffer &buf)
	{
		return path(buf, std::forward<A>(a)...);
	});
}
