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
	filesystem::path path(const vector_view<const string_view> &);
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
// fs/stdin.h
//

ircd::string_view
ircd::fs::stdin::readline(const mutable_buffer &buf)
{
	assert(ircd::ios);
	boost::asio::posix::stream_descriptor fd
	{
		*ircd::ios, dup(STDIN_FILENO)
	};

	boost::asio::streambuf sb
	{
		size(buf)
	};

	const auto interruption{[&fd]
	(ctx::ctx *const &interruptor) noexcept
	{
		fd.cancel();
	}};

	const size_t len
	{
		boost::asio::async_read_until(fd, sb, '\n', yield_context{to_asio{interruption}})
	};

	std::istream is{&sb};
	is.get(data(buf), size(buf), '\n');
	return string_view
	{
		data(buf), size_t(is.gcount())
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/read.h
//

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
	const fd fd
	{
		path
	};

	return read(fd, opts);
}
catch(const filesystem_error &)
{
	throw;
}
catch(const std::exception &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

std::string
ircd::fs::read(const fd &fd,
               const read_opts &opts)
{
	return string(size(fd), [&fd, &opts]
	(const mutable_buffer &buf)
	{
		return read(fd, buf, opts);
	});
}

ircd::const_buffer
ircd::fs::read(const string_view &path,
               const mutable_buffer &buf,
               const read_opts &opts)
try
{
	const fd fd
	{
		path
	};

	return read(fd, buf, opts);
}
catch(const filesystem_error &)
{
	throw;
}
catch(const std::exception &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

ircd::const_buffer
ircd::fs::read(const fd &fd,
               const mutable_buffer &buf,
               const read_opts &opts)
try
{
	#ifdef IRCD_USE_AIO
	if(likely(aioctx))
		return read__aio(fd, buf, opts);
	#endif

	return
	{
		data(buf),
		size_t(syscall(::pread, fd, data(buf), size(buf), opts.offset))
	};
}
catch(const filesystem_error &)
{
	throw;
}
catch(const std::exception &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/write.h
//

ircd::fs::write_opts
const ircd::fs::write_opts_default
{};

ircd::const_buffer
ircd::fs::append(const fd &fd,
                 const const_buffer &buf,
                 const write_opts &opts_)
try
{
	assert(opts_.offset == 0);

	auto opts(opts_);
	opts.offset = syscall(::lseek, fd, 0, SEEK_END);
	return write(fd, buf, opts);
}
catch(const filesystem_error &)
{
	throw;
}
catch(const std::exception &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

ircd::const_buffer
ircd::fs::write(const string_view &path,
                const const_buffer &buf,
                const write_opts &opts)
try
{
	const fd fd
	{
		path, std::ios::out
	};

	return write(fd, buf, opts);
}
catch(const filesystem_error &)
{
	throw;
}
catch(const std::exception &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

ircd::const_buffer
ircd::fs::write(const fd &fd,
                const const_buffer &buf,
                const write_opts &opts)
try
{
	#ifdef IRCD_USE_AIO
	if(likely(aioctx))
		return write__aio(fd, buf, opts);
	#endif

	return
	{
		data(buf),
		size_t(syscall(::pwrite, fd, data(buf), size(buf), opts.offset))
	};
}
catch(const filesystem_error &)
{
	throw;
}
catch(const std::exception &e)
{
	throw filesystem_error
	{
		"%s", e.what()
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/fd.h
//
// TODO: x-platform
//

namespace ircd::fs
{
	thread_local char path_buf[2048];
	static const char *path_str(const string_view &);
	static uint posix_flags(const std::ios::open_mode &mode);
}

size_t
ircd::fs::size(const fd &fd)
{
	struct stat stat;
	syscall(::fstat, fd, &stat);
	return stat.st_size;
};

uint
ircd::fs::posix_flags(const std::ios::open_mode &mode)
{
	static const auto rdwr
	{
		std::ios::in | std::ios::out
	};

	uint ret{0};
	if((mode & rdwr) == rdwr)
		ret |= O_RDWR;
	else if(mode & std::ios::out)
		ret |= O_WRONLY;
	else
		ret |= O_RDONLY;

	ret |= mode & std::ios::trunc? O_TRUNC : 0;
	ret |= mode & std::ios::app? O_APPEND : 0;
	ret |= ret & O_WRONLY? O_CREAT : 0;
	ret |= ret & O_RDWR && ret & (O_TRUNC | O_APPEND)? O_CREAT : 0;
	return ret;
}

const char *
ircd::fs::path_str(const string_view &s)
{
	return data(strlcpy(path_buf, s));
}

//
// fd::opts
//

ircd::fs::fd::opts::opts(const std::ios::open_mode &mode)
:flags
{
	posix_flags(mode)
}
,mask
{
	flags & O_CREAT?
		S_IRUSR | S_IWUSR:
		0U
}
,ate
{
	bool(mode & std::ios::ate)
}
{
}

//
// fd::fd
//

ircd::fs::fd::fd(const string_view &path)
:fd{path, opts{}}
{
}

ircd::fs::fd::fd(const string_view &path,
                 const opts &opts)
:fdno{[&path, &opts]
() -> int
{
	int flags(opts.flags);
	flags |= opts.direct? O_DIRECT : 0U;
	flags |= opts.cloexec? O_CLOEXEC : 0U;
	flags &= opts.nocreate? ~O_CREAT : flags;

	const mode_t &mode(opts.mask);
	const char *const &p(path_str(path));
	return syscall(::open, p, flags, mode);
}()}
{
	if(opts.ate)
		syscall(::lseek, fdno, 0, SEEK_END);
}

ircd::fs::fd::fd(fd &&o)
noexcept
:fdno
{
	std::move(o.fdno)
}
{
	o.fdno = -1;
}

ircd::fs::fd &
ircd::fs::fd::operator=(fd &&o)
noexcept
{
	this->~fd();
	fdno = std::move(o.fdno);
	o.fdno = -1;
	return *this;
}

ircd::fs::fd::~fd()
noexcept(false)
{
	if(fdno < 0)
		return;

	syscall(::close, fdno);
}

///////////////////////////////////////////////////////////////////////////////
//
// fs.h / misc
//

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
	boost::system::error_code ec;
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
	boost::system::error_code ec;
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
ircd::fs::make_path(const vector_view<const string_view> &list)
{
	filesystem::path ret;
	for(const auto &s : list)
		ret /= path(s);

	return ret.string();
}

filesystem::path
ircd::fs::path(const vector_view<const string_view> &list)
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
