// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_MAGIC_H
#include <boost/filesystem.hpp>
#include <ircd/asio.h>

#ifdef IRCD_USE_AIO
	#include "aio.h"
#endif

namespace ircd::fs
{
	using namespace boost::filesystem;

	enum { NAME, PATH };
	using ent = std::pair<std::string, std::string>;
	extern const std::array<ent, num_of<index>()> paths;
}

namespace ircd::fs::magic
{
	static void init();
	static void fini();
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
	magic::init();

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
	magic::fini();
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
{
	#ifdef IRCD_USE_AIO
	if(likely(aioctx))
		return read__aio(path, opts);
	#endif

	return read__std(path, opts);
}

ircd::string_view
ircd::fs::read(const string_view &path,
               const mutable_buffer &buf,
               const read_opts &opts)
{
	#ifdef IRCD_USE_AIO
	if(likely(aioctx))
		return read__aio(path, buf, opts);
	#endif

	return read__std(path, buf, opts);
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
{
	#ifdef IRCD_USE_AIO
	if(likely(aioctx))
		return write__aio(path, buf, opts);
	#endif

	return write__std(path, buf, opts);
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
ircd::fs::chdir(const std::string &path)
try
{
	fs::current_path(path);
}
catch(const fs::filesystem_error &e)
{
	throw filesystem_error("%s", e.what());
}

bool
ircd::fs::mkdir(const std::string &path)
try
{
	return fs::create_directory(path);
}
catch(const fs::filesystem_error &e)
{
	throw filesystem_error("%s", e.what());
}

std::string
ircd::fs::cwd()
try
{
	return fs::current_path().string();
}
catch(const fs::filesystem_error &e)
{
	throw filesystem_error("%s", e.what());
}

std::vector<std::string>
ircd::fs::ls_recursive(const std::string &path)
try
{
	const fs::recursive_directory_iterator end;
	fs::recursive_directory_iterator it(path);

	std::vector<std::string> ret;
	std::transform(it, end, std::back_inserter(ret), []
	(const auto &ent)
	{
		return ent.path().string();
	});

	return ret;
}
catch(const fs::filesystem_error &e)
{
	throw filesystem_error("%s", e.what());
}

std::vector<std::string>
ircd::fs::ls(const std::string &path)
try
{
	static const fs::directory_iterator end;
	fs::directory_iterator it(path);

	std::vector<std::string> ret;
	std::transform(it, end, std::back_inserter(ret), []
	(const auto &ent)
	{
		return ent.path().string();
	});

	return ret;
}
catch(const fs::filesystem_error &e)
{
	throw filesystem_error("%s", e.what());
}

size_t
ircd::fs::size(const string_view &path)
{
	return ircd::fs::size(std::string{path});
}

size_t
ircd::fs::size(const std::string &path)
{
	return fs::file_size(path);
}

bool
ircd::fs::is_reg(const std::string &path)
try
{
	return fs::is_regular_file(path);
}
catch(const fs::filesystem_error &e)
{
	throw filesystem_error("%s", e.what());
}

bool
ircd::fs::is_dir(const std::string &path)
try
{
	return fs::is_directory(path);
}
catch(const fs::filesystem_error &e)
{
	throw filesystem_error("%s", e.what());
}

bool
ircd::fs::exists(const std::string &path)
try
{
	return boost::filesystem::exists(path);
}
catch(const fs::filesystem_error &e)
{
	throw filesystem_error("%s", e.what());
}

std::string
ircd::fs::make_path(const std::initializer_list<std::string> &list)
{
	fs::path ret;
	for(const auto &s : list)
		ret /= fs::path(s);

	return ret.string();
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

///////////////////////////////////////////////////////////////////////////////
//
// fs/magic.h
//

namespace ircd::fs::magic
{
	magic_t cookie;

	// magic_getflags() may not be available; this will supplement for our
	// tracking of their flags state.
	int flags{MAGIC_NONE};

	static void throw_on_error(const magic_t &);
	static void set_flags(const magic_t &, const int &flags);
	static string_view call(const magic_t &, const int &flags, const mutable_buffer &, const std::function<const char *()> &);
	static string_view call(const magic_t &, const int &flags, const mutable_buffer &, const const_buffer &);
}

void
ircd::fs::magic::init()
{
	if(unlikely(!(cookie = ::magic_open(flags))))
		throw error{"magic_open() failed"};

	//TODO: conf; TODO: windows
	static const char *const paths[]
	{
		"/usr/local/share/misc/magic.mgc",
		"/usr/share/misc/magic.mgc",
		nullptr
	};

	for(const char *const *path{paths}; *path; ++path)
		if(magic_check(cookie, *path) == 0)
			if(magic_load(cookie, *path) == 0)
				return;

	throw error
	{
		"Failed to open any magic database"
	};
}

void
ircd::fs::magic::fini()
{
	::magic_close(cookie);
}

ircd::string_view
ircd::fs::magic::description(const mutable_buffer &out,
                             const const_buffer &buffer)
{
	return call(cookie, MAGIC_NONE, out, buffer);
}

ircd::string_view
ircd::fs::magic::extensions(const mutable_buffer &out,
                            const const_buffer &buffer)
{
	return call(cookie, MAGIC_EXTENSION, out, buffer);
}

ircd::string_view
ircd::fs::magic::mime_encoding(const mutable_buffer &out,
                               const const_buffer &buffer)
{
	return call(cookie, MAGIC_MIME_ENCODING, out, buffer);
}

ircd::string_view
ircd::fs::magic::mime_type(const mutable_buffer &out,
                           const const_buffer &buffer)
{
	return call(cookie, MAGIC_MIME_TYPE, out, buffer);
}

ircd::string_view
ircd::fs::magic::mime(const mutable_buffer &out,
                      const const_buffer &buffer)
{
	return call(cookie, MAGIC_MIME, out, buffer);
}

ircd::string_view
ircd::fs::magic::call(const magic_t &cookie,
                      const int &flags,
                      const mutable_buffer &out,
                      const const_buffer &buffer)
{
	return call(cookie, flags, out, [&cookie, &buffer]
	{
		return magic_buffer(cookie, data(buffer), size(buffer));
	});
}

ircd::string_view
ircd::fs::magic::call(const magic_t &cookie,
                      const int &ours,
                      const mutable_buffer &out,
                      const std::function<const char *()> &closure)
{
	const auto &theirs{magic::flags};
	const unwind reset{[&theirs, &cookie]
	{
		set_flags(cookie, theirs);
	}};

	set_flags(cookie, ours);
	string_view str
	{
		closure()
	};

	if(!str)
	{
		throw_on_error(cookie);
		str = "application/octet-stream"_sv;
	}

	return { data(out), copy(out, str) };
}

void
ircd::fs::magic::set_flags(const magic_t &cookie,
                           const int &flags)
{
	if(magic_setflags(cookie, flags) == -1)
		throw_on_error(cookie);
}

void
ircd::fs::magic::throw_on_error(const magic_t &cookie)
{
	const char *const errstr
	{
		::magic_error(cookie)
	};

	if(errstr)
		throw error
		{
			"%s", errstr
		};

	assert(magic_errno(cookie) == 0);
}

int
ircd::fs::magic::version()
{
	return ::magic_version();
}

__attribute__((constructor))
static void
ircd_fs_magic_version_check()
{
	if(unlikely(MAGIC_VERSION != ircd::fs::magic::version()))
	{
		fprintf(stderr, "FATAL: linked libmagic version %d != compiled magic.h version %d.\n",
		        ircd::fs::magic::version(),
		        MAGIC_VERSION);

		std::terminate();
	}
}
