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

// Filesystem path utility interface
namespace ircd::fs
{
	using path_views = vector_view<const string_view>;
	using path_strings = vector_view<const std::string>;

	extern const size_t NAME_MAX_LEN;
	extern const size_t PATH_MAX_LEN;

	// convenience tls buffers of appropriate size.
	extern const mutable_buffer path_scratch;
	extern const mutable_buffer name_scratch;

	// must include boost in unit to call these; use path() instead
	filesystem::path _path(std::string);
	filesystem::path _path(const string_view &);
	filesystem::path _path(const path_views &);
	filesystem::path _path(const path_strings &);

	// append path strings together to create a viable result amalgam.
	string_view path(const mutable_buffer &, const path_views &);
	string_view path(const mutable_buffer &, const path_strings &);
	string_view path(const mutable_buffer &, const filesystem::path &);

	// guarantees result is contained within the base, mitigating `../` etc.
	string_view path(const mutable_buffer &, const string_view &base, const path_views &);

	// wrappers for path()
	template<class... A> std::string path_string(A&&...);
	const char *path_cstr(const string_view &path); // rotating internal TLS buffer

	// pathconf(3) interface
	long pathconf(const string_view &path, const int &arg);
	size_t name_max_len(const string_view &path);
	size_t path_max_len(const string_view &path);

	// get current working directory of the process.
	string_view cwd(const mutable_buffer &buf);
	std::string cwd();
}

// Filesystem path tool and conveniences interface.
namespace ircd::fs
{
	bool is_relative(const string_view &path);
	bool is_absolute(const string_view &path);

	string_view extension(const mutable_buffer &, const string_view &path, const string_view &replace);
	string_view extension(const mutable_buffer &, const string_view &path);
	string_view filename(const mutable_buffer &, const string_view &path);
	string_view parent(const mutable_buffer &, const string_view &path);

	string_view canonical(const mutable_buffer &, const string_view &path);
	string_view canonical(const mutable_buffer &, const string_view &root, const string_view &path);
	string_view relative(const mutable_buffer &, const string_view &root, const string_view &path);
	string_view absolute(const mutable_buffer &, const string_view &root, const string_view &path);
}

/// Configuration items storing the base paths used at runtime for program
/// operation. The defaults are generated at ./configure time and obtained
/// from macros in config.h. As conf items, these values may be overriden by
/// environment variables and may be updated by conf loads from the database.
///
namespace ircd::fs::base
{
	extern conf::item<std::string> prefix;      // e.g. /usr
	extern conf::item<std::string> bin;         // e.g. /usr/bin
	extern conf::item<std::string> etc;         // e.g. /etc
	extern conf::item<std::string> include;     // e.g. /usr/include/ircd
	extern conf::item<std::string> lib;         // e.g. /usr/lib
	extern conf::item<std::string> modules;     // e.g. /usr/lib/modules/ircd
	extern conf::item<std::string> share;       // e.g. /usr/share/ircd
	extern conf::item<std::string> run;         // e.g. /var/run/ircd
	extern conf::item<std::string> log;         // e.g. /var/log/ircd
	extern conf::item<std::string> db;          // e.g. /var/db/ircd
}

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
