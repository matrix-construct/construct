// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <boost/filesystem.hpp>
#include <ircd/asio.h>

#ifdef IRCD_USE_AIO
	#include "aio.h"
#endif

namespace filesystem = boost::filesystem;

namespace ircd::fs
{
	enum { NAME, PATH };
	using ent = std::pair<std::string, std::string>;
	extern const std::array<ent, num_of<index>()> paths;

	filesystem::path path(std::string);
	filesystem::path path(const string_view &);
	filesystem::path path(const std::initializer_list<string_view> &);
}

/// Non-null when aio is available for use
decltype(ircd::fs::aioctx)
ircd::fs::aioctx
{};

decltype(ircd::fs::paths)
ircd::fs::paths
{{
	{ "prefix",            DPATH         },
	{ "binary dir",        BINPATH       },
	{ "config",            ETCPATH       },
	{ "log",               LOGPATH       },
	{ "libexec dir",       PKGLIBEXECDIR },
	{ "modules",           MODPATH       },
	{ "ircd.conf",         CPATH         },
	{ "ircd binary",       SPATH         },
	{ "db",                DBPATH        },
}};

ircd::fs::init::init()
{
	#ifdef IRCD_USE_AIO
		assert(!aioctx);
		aioctx = new aio{};
	#else
		log::warning
		{
			"No support for asynchronous local filesystem IO..."
		};
	#endif
}

ircd::fs::init::~init()
noexcept
{
	#ifdef IRCD_USE_AIO
		delete aioctx;
		aioctx = nullptr;
	#endif

	assert(!aioctx);
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/read.h
//

namespace ircd::fs
{
	string_view read__std(const string_view &path, const mutable_buffer &, const read_opts &);
	std::string read__std(const string_view &path, const read_opts &);
}

ircd::fs::read_opts
const ircd::fs::read_opts_default
{};

//
// ircd::fs interface linkage
//

std::string
ircd::fs::read(const string_view &path,
               const read_opts &opts)
try
{
	#ifdef IRCD_USE_AIO
	if(likely(aioctx))
		return read__aio(path, opts);
	#endif

	return read__std(path, opts);
}
catch(const std::exception &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

ircd::string_view
ircd::fs::read(const string_view &path,
               const mutable_buffer &buf,
               const read_opts &opts)
try
{
	#ifdef IRCD_USE_AIO
	if(likely(aioctx))
		return read__aio(path, buf, opts);
	#endif

	return read__std(path, buf, opts);
}
catch(const std::exception &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

//
// std read
//

std::string
ircd::fs::read__std(const string_view &path,
                    const read_opts &opts)
{
	std::ifstream file{std::string{path}};
	std::noskipws(file);
	std::istream_iterator<char> b{file};
	std::istream_iterator<char> e{};
	return std::string{b, e};
}

ircd::string_view
ircd::fs::read__std(const string_view &path,
                    const mutable_buffer &buf,
                    const read_opts &opts)
{
	std::ifstream file{std::string{path}};
	file.exceptions(file.failbit | file.badbit);
	file.seekg(opts.offset, file.beg);
	file.read(data(buf), size(buf));
	return
	{
		data(buf), size_t(file.gcount())
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/write.h
//

namespace ircd::fs
{
	string_view write__std(const string_view &path, const const_buffer &, const write_opts &);
}

ircd::fs::write_opts
const ircd::fs::write_opts_default
{};

ircd::string_view
ircd::fs::write(const string_view &path,
                const const_buffer &buf,
                const write_opts &opts)
try
{
	#ifdef IRCD_USE_AIO
	if(likely(aioctx))
		return write__aio(path, buf, opts);
	#endif

	return write__std(path, buf, opts);
}
catch(const std::exception &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

ircd::string_view
ircd::fs::write__std(const string_view &path,
                     const const_buffer &buf,
                     const write_opts &opts)
{
	const auto open_mode
	{
		opts.append?
			std::ios::app:

		opts.overwrite?
			std::ios::trunc:

		std::ios::out
	};

	std::ofstream file
	{
		std::string{path}, open_mode
	};

	file.seekp(opts.offset, file.beg);
	file.write(data(buf), size(buf));
	return buf;
}

void
ircd::fs::chdir(const string_view &path)
try
{
	filesystem::current_path(fs::path(path));
}
catch(const filesystem::filesystem_error &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

bool
ircd::fs::mkdir(const string_view &path)
try
{
	return filesystem::create_directory(fs::path(path));
}
catch(const filesystem::filesystem_error &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

std::string
ircd::fs::cwd()
try
{
	return filesystem::current_path().string();
}
catch(const filesystem::filesystem_error &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

bool
ircd::fs::remove(const string_view &path)
try
{
	return filesystem::remove(fs::path(path));
}
catch(const filesystem::filesystem_error &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

bool
ircd::fs::remove(std::nothrow_t,
                 const string_view &path)
{
	error_code ec;
	return filesystem::remove(fs::path(path), ec);
}

void
ircd::fs::rename(const string_view &old,
                 const string_view &new_)
try
{
	filesystem::rename(path(old), path(new_));
}
catch(const filesystem::filesystem_error &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

bool
ircd::fs::rename(std::nothrow_t,
                 const string_view &old,
                 const string_view &new_)
{
	error_code ec;
	filesystem::rename(path(old), path(new_), ec);
	return !ec;
}

std::vector<std::string>
ircd::fs::ls_recursive(const string_view &path)
try
{
	const filesystem::recursive_directory_iterator end;
	filesystem::recursive_directory_iterator it
	{
		fs::path(path)
	};

	std::vector<std::string> ret;
	std::transform(it, end, std::back_inserter(ret), []
	(const auto &ent)
	{
		return ent.path().string();
	});

	return ret;
}
catch(const filesystem::filesystem_error &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

std::vector<std::string>
ircd::fs::ls(const string_view &path)
try
{
	static const filesystem::directory_iterator end;
	filesystem::directory_iterator it
	{
		fs::path(path)
	};

	std::vector<std::string> ret;
	std::transform(it, end, std::back_inserter(ret), []
	(const auto &ent)
	{
		return ent.path().string();
	});

	return ret;
}
catch(const filesystem::filesystem_error &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

size_t
ircd::fs::size(const string_view &path)
{
	return filesystem::file_size(fs::path(path));
}

bool
ircd::fs::is_reg(const string_view &path)
try
{
	return filesystem::is_regular_file(fs::path(path));
}
catch(const filesystem::filesystem_error &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

bool
ircd::fs::is_dir(const string_view &path)
try
{
	return filesystem::is_directory(fs::path(path));
}
catch(const filesystem::filesystem_error &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

bool
ircd::fs::exists(const string_view &path)
try
{
	return filesystem::exists(fs::path(path));
}
catch(const filesystem::filesystem_error &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

std::string
ircd::fs::make_path(const std::initializer_list<string_view> &list)
{
	filesystem::path ret;
	for(const auto &s : list)
		ret /= path(s);

	return ret.string();
}

filesystem::path
ircd::fs::path(const std::initializer_list<string_view> &list)
{
	filesystem::path ret;
	for(const auto &s : list)
		ret /= path(s);

	return ret.string();
}

filesystem::path
ircd::fs::path(const string_view &s)
{
	return path(std::string{s});
}

filesystem::path
ircd::fs::path(std::string s)
{
	return filesystem::path(std::move(s));
}

const char *
ircd::fs::get(index index)
noexcept try
{
	return std::get<PATH>(paths.at(index)).c_str();
}
catch(const std::out_of_range &e)
{
	return nullptr;
}

const char *
ircd::fs::name(index index)
noexcept try
{
	return std::get<NAME>(paths.at(index)).c_str();
}
catch(const std::out_of_range &e)
{
	return nullptr;
}
