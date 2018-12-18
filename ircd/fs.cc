// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_SYS_STAT_H
#include <boost/filesystem.hpp>
#include <ircd/asio.h>

#ifdef IRCD_USE_AIO
	#include "aio.h"
#endif

namespace filesystem = boost::filesystem;

namespace ircd::fs
{
	filesystem::path path(std::string);
	filesystem::path path(const string_view &);
	filesystem::path path(const vector_view<const string_view> &);
}

//
// init
//

ircd::fs::init::init()
{
}

ircd::fs::init::~init()
noexcept
{
}

//
// Compile-time path index
//

namespace ircd::fs
{
	struct sysent;
	extern const std::array<struct sysent, num_of<index>()> syspaths;
}

struct ircd::fs::sysent
{
	string_view name;
	string_view path;
};

decltype(ircd::fs::syspaths)
ircd::fs::syspaths
{{
	{ "installation prefix",      RB_PREFIX      },
	{ "binary directory",         RB_BIN_DIR     },
	{ "configuration directory",  RB_CONF_DIR    },
	{ "data directory",           RB_DATA_DIR    },
	{ "database directory",       RB_DB_DIR      },
	{ "log directory",            RB_LOG_DIR     },
	{ "module directory",         RB_MODULE_DIR  },
}};

ircd::string_view
ircd::fs::get(index index)
noexcept try
{
	return syspaths.at(index).path;
}
catch(const std::out_of_range &e)
{
	return {};
}

ircd::string_view
ircd::fs::name(index index)
noexcept try
{
	return syspaths.at(index).name;
}
catch(const std::out_of_range &e)
{
	return {};
}

///////////////////////////////////////////////////////////////////////////////
//
// fs.h / misc
//

std::string
ircd::fs::cwd()
try
{
	return filesystem::current_path().string();
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

void
ircd::fs::chdir(const string_view &path)
try
{
	filesystem::current_path(fs::path(path));
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

bool
ircd::fs::mkdir(const string_view &path)
try
{
	return filesystem::create_directories(fs::path(path));
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

bool
ircd::fs::remove(const string_view &path)
try
{
	return filesystem::remove(fs::path(path));
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
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
	throw error{e};
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
	throw error{e};
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
	throw error{e};
}

size_t
ircd::fs::size(const string_view &path)
try
{
	return filesystem::file_size(fs::path(path));
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

bool
ircd::fs::is_reg(const string_view &path)
try
{
	return filesystem::is_regular_file(fs::path(path));
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

bool
ircd::fs::is_dir(const string_view &path)
try
{
	return filesystem::is_directory(fs::path(path));
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

bool
ircd::fs::exists(const string_view &path)
try
{
	return filesystem::exists(fs::path(path));
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

std::string
ircd::fs::make_path(const vector_view<const std::string> &list)
try
{
	filesystem::path ret;
	for(const auto &s : list)
		ret /= path(s);

	return ret.string();
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

std::string
ircd::fs::make_path(const vector_view<const string_view> &list)
try
{
	filesystem::path ret;
	for(const auto &s : list)
		ret /= path(s);

	return ret.string();
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

filesystem::path
ircd::fs::path(const vector_view<const string_view> &list)
try
{
	filesystem::path ret;
	for(const auto &s : list)
		ret /= path(s);

	return ret.string();
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

filesystem::path
ircd::fs::path(const string_view &s)
try
{
	return path(std::string{s});
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

filesystem::path
ircd::fs::path(std::string s)
try
{
	return filesystem::path(std::move(s));
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/support.h
//

bool
ircd::fs::support::fallocate(const string_view &path,
                             const write_opts &wopts)
try
{
	const fd::opts opts
	{
		std::ios::out
	};

	fs::fd fd
	{
		path, opts
	};

	fs::allocate(fd, info::page_size, wopts);
	return true;
}
catch(const std::system_error &e)
{
	const auto &ec(e.code());
	if(system_category(ec)) switch(ec.value())
	{
		case int(std::errc::invalid_argument):
		case int(std::errc::operation_not_supported):
			return false;

		default:
			break;
	}

	throw;
}

bool
ircd::fs::support::direct_io(const string_view &path)
try
{
	fd::opts opts{std::ios::out};
	opts.direct = true;
	fd{path, opts};
	return true;
}
catch(const std::system_error &e)
{
	const auto &ec(e.code());
	if(system_category(ec)) switch(ec.value())
	{
		case int(std::errc::invalid_argument):
			return false;

		default:
			break;
	}

	throw;
}

/// True if AIO is supported by this build and runtime.
decltype(ircd::fs::support::aio)
ircd::fs::support::aio
{
	#ifdef IRCD_USE_AIO
		true
	#else
		false
	#endif
};

/// True if IOCB_CMD_FSYNC is supported by AIO. If this is false then
/// fs::fsync_opts::async=true flag is ignored.
decltype(ircd::fs::support::aio_fsync)
ircd::fs::support::aio_fsync
{
	false //TODO: Detect kernel support
};

/// True if IOCB_CMD_FDSYNC is supported by AIO. If this is false then
/// fs::fsync_opts::async=true flag is ignored.
decltype(ircd::fs::support::aio_fdsync)
ircd::fs::support::aio_fdsync
{
	false //TODO: Detect kernel support
};

///////////////////////////////////////////////////////////////////////////////
//
// fs/stdin.h
//

ircd::string_view
ircd::fs::stdin::readline(const mutable_buffer &buf)
try
{
	boost::asio::posix::stream_descriptor fd
	{
		ios::get(), dup(STDIN_FILENO)
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
catch(boost::system::system_error &e)
{
	throw_system_error(e);
	__builtin_unreachable();
}

//
// tty
//

ircd::fs::stdin::tty::tty()
:fd{[]
{
	thread_local char buf[256];
	syscall(::ttyname_r, STDIN_FILENO, buf, sizeof(buf));
	return fd
	{
		string_view{buf}, std::ios_base::out
	};
}()}
{
}

size_t
ircd::fs::stdin::tty::write(const string_view &buf)
{
	return syscall(::write, int(*this), buf.data(), buf.size());
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/sync.h
//

ircd::fs::sync_opts
const ircd::fs::sync_opts_default;

void
ircd::fs::sync(const fd &fd,
               const off_t &offset,
               const size_t &length,
               const sync_opts &opts)
{
	return sync(fd, opts);
}

void
ircd::fs::sync(const fd &fd,
               const sync_opts &opts)
{
	const ctx::slice_usage_warning message
	{
		"fs::sync(fd:%d)", int(fd)
	};

	#ifdef __linux__
		syscall(::syncfs, fd);
	#else
		syscall(::sync);
	#endif
}

void
ircd::fs::flush(const fd &fd,
                const off_t &offset,
                const size_t &length,
                const sync_opts &opts)
{
	return flush(fd, opts);
}

void
ircd::fs::flush(const fd &fd,
                const sync_opts &opts)
{
	const ctx::slice_usage_warning message
	{
		"fs::flush(fd:%d, {metadata:%b aio:%b:%b})",
		int(fd),
		opts.metadata,
		opts.aio,
		opts.metadata? support::aio_fdsync : support::aio_fsync
	};

	#ifdef IRCD_USE_AIO
	if(aio::context && opts.aio)
	{
		if(!opts.metadata && support::aio_fdsync)
			return aio::fdsync(fd, opts);

		if(support::aio_fsync)
			return aio::fsync(fd, opts);
	}
	#endif

	if(!opts.metadata)
		return void(syscall(::fdatasync, fd));

	return void(syscall(::fsync, fd));
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/prefetch.h
//

///////////////////////////////////////////////////////////////////////////////
//
// fs/read.h
//

namespace ircd::fs
{
	static size_t read(const fd &, const const_iovec_view &, const read_opts &);
}

ircd::fs::read_opts
const ircd::fs::read_opts_default
{};

#ifdef __linux__
void
ircd::fs::prefetch(const fd &fd,
                   const size_t &count,
                   const read_opts &opts)
{
	const auto flags
	{
		syscall(::fcntl, fd, F_GETFL)
	};

	if(~flags & O_DIRECT)
	{
		syscall(::readahead, fd, opts.offset, count);
		return;
	}

	#ifdef IRCD_USE_AIO
	if(likely(aio::context) && opts.aio)
		aio::prefetch(fd, count, opts);
	#endif
}
#else
void
ircd::fs::prefetch(const fd &fd,
                   const size_t &count,
                   const read_opts &opts)
{
}
#endif

std::string
ircd::fs::read(const string_view &path,
               const read_opts &opts)
{
	const fd fd
	{
		path
	};

	return read(fd, opts);
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
{
	const mutable_buffers bufs
	{
		&buf, 1
	};

	return mutable_buffer
	{
		data(buf), read(path, bufs, opts)
	};
}

ircd::const_buffer
ircd::fs::read(const fd &fd,
               const mutable_buffer &buf,
               const read_opts &opts)
{
	const mutable_buffers bufs
	{
		&buf, 1
	};

	return mutable_buffer
	{
		data(buf), read(fd, bufs, opts)
	};
}

size_t
ircd::fs::read(const string_view &path,
               const mutable_buffers &bufs,
               const read_opts &opts)
{
	const fd fd
	{
		path
	};

	return read(fd, bufs, opts);
}

size_t
ircd::fs::read(const fd &fd,
               const mutable_buffers &bufs,
               const read_opts &opts_)
{
	size_t ret(0);
	read_opts opts(opts_); do
	{
		assert(opts.offset >= opts_.offset);
		const size_t off(opts.offset - opts_.offset);
		assert(off <= buffers::size(bufs));
		assert(ret <= buffers::size(bufs));
		const auto iov(make_iov(bufs, ret));
		ret += read(fd, iov, opts);
		if(!opts_.all)
			break;

		if(off >= ret)
			break;

		opts.offset = opts_.offset + ret;
	}
	while(ret < buffers::size(bufs));
	assert(opts.offset >= opts_.offset);
	assert(ret <= buffers::size(bufs));
	return ret;
}

size_t
ircd::fs::read(const fd &fd,
               const const_iovec_view &iov,
               const read_opts &opts)
{
	#ifdef IRCD_USE_AIO
	if(aio::context && opts.aio)
		return aio::read(fd, iov, opts);
	#endif

	return syscall(::preadv, fd, iov.data(), iov.size(), opts.offset);
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/write.h
//

ircd::fs::write_opts
const ircd::fs::write_opts_default
{};

void
ircd::fs::allocate(const fd &fd,
                   const size_t &size,
                   const write_opts &opts)
{
	int mode{0};
	mode |= opts.keep_size? FALLOC_FL_KEEP_SIZE : 0;
	syscall(::fallocate, fd, mode, opts.offset, size);
}

void
ircd::fs::truncate(const string_view &path,
                   const size_t &size,
                   const write_opts &opts)
{
	const fd fd
	{
		path, std::ios::out | std::ios::trunc
	};

	return truncate(fd, size, opts);
}

void
ircd::fs::truncate(const fd &fd,
                   const size_t &size,
                   const write_opts &opts)
{
	syscall(::ftruncate, fd, size);
}

ircd::const_buffer
ircd::fs::overwrite(const string_view &path,
                    const const_buffer &buf,
                    const write_opts &opts)
{
	const const_buffers bufs
	{
		&buf, 1
	};

	return const_buffer
	{
		data(buf), overwrite(path, bufs, opts)
	};
}

ircd::const_buffer
ircd::fs::overwrite(const fd &fd,
                    const const_buffer &buf,
                    const write_opts &opts)
{
	const const_buffers bufs
	{
		&buf, 1
	};

	return const_buffer
	{
		data(buf), overwrite(fd, bufs, opts)
	};
}

size_t
ircd::fs::overwrite(const string_view &path,
                    const const_buffers &bufs,
                    const write_opts &opts)
{
	const fd fd
	{
		path, std::ios::out | std::ios::trunc
	};

	return overwrite(fd, bufs, opts);
}

size_t
ircd::fs::overwrite(const fd &fd,
                    const const_buffers &bufs,
                    const write_opts &opts)
{
	return write(fd, bufs, opts);
}

ircd::const_buffer
ircd::fs::append(const string_view &path,
                 const const_buffer &buf,
                 const write_opts &opts)
{
	const const_buffers bufs
	{
		&buf, 1
	};

	return const_buffer
	{
		data(buf), append(path, bufs, opts)
	};
}

ircd::const_buffer
ircd::fs::append(const fd &fd,
                 const const_buffer &buf,
                 const write_opts &opts)
{
	const const_buffers bufs
	{
		&buf, 1
	};

	return const_buffer
	{
		data(buf), append(fd, bufs, opts)
	};
}

size_t
ircd::fs::append(const string_view &path,
                 const const_buffers &bufs,
                 const write_opts &opts)
{
	const fd fd
	{
		path, std::ios::out | std::ios::app
	};

	return append(fd, bufs, opts);
}

size_t
ircd::fs::append(const fd &fd,
                 const const_buffers &bufs,
                 const write_opts &opts_)
{
	auto opts(opts_);
	if(!opts.offset)
		opts.offset = syscall(::lseek, fd, 0, SEEK_END);

	return write(fd, bufs, opts);
}

ircd::const_buffer
ircd::fs::write(const string_view &path,
                const const_buffer &buf,
                const write_opts &opts)
{
	const const_buffers bufs
	{
		&buf, 1
	};

	return const_buffer
	{
		data(buf), write(path, bufs, opts)
	};
}

ircd::const_buffer
ircd::fs::write(const fd &fd,
                const const_buffer &buf,
                const write_opts &opts)
{
	const const_buffers bufs
	{
		&buf, 1
	};

	return const_buffer
	{
		data(buf), write(fd, bufs, opts)
	};
}

size_t
ircd::fs::write(const string_view &path,
                const const_buffers &bufs,
                const write_opts &opts)
{
	const fd fd
	{
		path, std::ios::out
	};

	return write(fd, bufs, opts);
}

size_t
ircd::fs::write(const fd &fd,
                const const_buffers &bufs,
                const write_opts &opts)
{
	const auto iov(make_iov(bufs));

	#ifdef IRCD_USE_AIO
	if(likely(aio::context) && opts.aio)
		return aio::write(fd, iov, opts);
	#endif

	return size_t(syscall(::pwritev, fd, iov.data(), iov.size(), opts.offset));
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/aio.h
//

/// Conf item to control whether AIO is enabled or bypassed.
decltype(ircd::fs::aio::enable)
ircd::fs::aio::enable
{
	{ "name",     "ircd.fs.aio.enable"  },
	{ "default",  true                  },
	{ "persist",  false                 },
};

/// Global stats structure
decltype(ircd::fs::aio::stats)
ircd::fs::aio::stats;

/// Non-null when aio is available for use
decltype(ircd::fs::aio::context)
ircd::fs::aio::context;

//
// init
//

#ifdef IRCD_USE_AIO
ircd::fs::aio::init::init()
{
	assert(!context);
	if(!bool(aio::enable))
		return;

	context = new kernel{};
}
#else
ircd::fs::aio::init::init()
{
	assert(!context);
	log::warning
	{
		"No support for asynchronous local filesystem IO..."
	};
}
#endif

#ifdef IRCD_USE_AIO
ircd::fs::aio::init::~init()
noexcept
{
	delete context;
	context = nullptr;
}
#else
ircd::fs::aio::init::~init()
noexcept
{
	assert(!context);
}
#endif

///////////////////////////////////////////////////////////////////////////////
//
// fs/fd.h
//
// TODO: x-platform
//

namespace ircd::fs
{
	thread_local char path_buf[PATH_MAX];
	static const char *path_str(const string_view &);
	static uint posix_flags(const std::ios::openmode &mode);
}

decltype(ircd::fs::fd::opts::direct_io_enable)
ircd::fs::fd::opts::direct_io_enable
{
	{ "name",     "ircd.fs.fd.direct_io_enable"  },
	{ "default",  true                           },
	{ "persist",  false                          },
};

#ifdef __linux__
size_t
ircd::fs::block_size(const fd &fd)
{
	return 512UL;
}
#elif defined(HAVE_SYS_STAT_H)
size_t
ircd::fs::block_size(const fd &fd)
{
	struct stat st;
	syscall(::fstat, fd, &st);
	return st.st_blksize;
}
#else
size_t
ircd::fs::block_size(const fd &fd)
{
	return info::page_size;
}
#endif

/// This is not a standard UUID of any sort; this is custom, and intended for
/// rocksdb (though rocksdb has no requirement for its format specifically).
/// The format is plain-text, fs major and minor number, inode number, and
/// a three letter file type; all obtained from fstat(2).
ircd::string_view
ircd::fs::uuid(const fd &fd,
               const mutable_buffer &buf)
{
	struct stat stat;
	syscall(::fstat, fd, &stat);
	return fmt::sprintf
	{
		buf, "%u-%u-%lu-%s",
		gnu_dev_major(stat.st_dev),
		gnu_dev_minor(stat.st_dev),
		stat.st_ino,
		S_ISREG(stat.st_mode)? "reg":
		S_ISDIR(stat.st_mode)? "dir":
		                       "???"
	};
}

size_t
ircd::fs::size(const fd &fd)
{
	const off_t cur
	{
		syscall(::lseek, fd, 0, SEEK_CUR)
	};

	const off_t end
	{
		syscall(::lseek, fd, 0, SEEK_END)
	};

	syscall(::lseek, fd, cur, SEEK_SET);
	return end;
}

uint
ircd::fs::posix_flags(const std::ios::openmode &mode)
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

ircd::fs::fd::opts::opts(const std::ios::openmode &mode)
:mode
{
	mode
}
,flags
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
	uint flags(opts.flags);
	flags |= opts.direct? O_DIRECT : 0UL;
	flags |= opts.cloexec? O_CLOEXEC : 0UL;
	flags &= opts.nocreate? ~O_CREAT : flags;

	const mode_t &mode(opts.mask);
	assert((flags & ~O_CREAT) || mode != 0);

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
// fs/iov.h
//

namespace ircd::fs
{
	thread_local std::array<struct ::iovec, IOV_MAX> _iov_;
}

ircd::fs::const_iovec_view
ircd::fs::make_iov(const mutable_buffers &bufs,
                   const size_t &offset)
{
	if(unlikely(bufs.size() > _iov_.size()))
		throw error
		{
			make_error_code(std::errc::invalid_argument),
			"Buffer count of %zu exceeds IOV_MAX of %zu",
			bufs.size(),
			_iov_.size()
		};

	return make_iov(iovec_view(_iov_.data(), _iov_.size()), bufs, offset);
}

ircd::fs::const_iovec_view
ircd::fs::make_iov(const const_buffers &bufs,
                   const size_t &offset)
{
	if(unlikely(bufs.size() > _iov_.size()))
		throw error
		{
			make_error_code(std::errc::invalid_argument),
			"Buffer count of %zu exceeds IOV_MAX of %zu",
			bufs.size(),
			_iov_.size()
		};

	return make_iov(iovec_view(_iov_.data(), _iov_.size()), bufs, offset);
}

ircd::fs::const_iovec_view
ircd::fs::make_iov(const iovec_view &iov,
                   const mutable_buffers &bufs,
                   const size_t &offset)
{
	const size_t max
	{
		std::min(iov.size(), bufs.size())
	};

	size_t i(0), off(0);
	for(; i < max; off += size(bufs[i++]))
		if(size(bufs[i]) > offset - off)
		{
			off = offset - off;
			break;
		}

	if(i < max)
	{
		assert(off <= size(bufs[i]));
		iov.at(i) =
		{
			data(bufs[i]) + off, size(bufs[i]) - off
		};

		for(++i; i < max; ++i)
			iov.at(i) =
			{
				data(bufs[i]), size(bufs[i])
			};
	}

	const const_iovec_view ret{iov.data(), i};
	assert(bytes(ret) <= buffer::buffers::size(bufs));
	return ret;
}

ircd::fs::const_iovec_view
ircd::fs::make_iov(const iovec_view &iov,
                   const const_buffers &bufs,
                   const size_t &offset)
{
	const size_t max
	{
		std::min(iov.size(), bufs.size())
	};

	size_t i(0), off(0);
	for(; i < max; off += size(bufs[i++]))
		if(size(bufs[i]) > offset - off)
		{
			off = offset - off;
			break;
		}

	if(i < max)
	{
		assert(off <= size(bufs[i]));
		iov.at(i) =
		{
			const_cast<char *>(data(bufs[i])) + off, size(bufs[i]) - off
		};

		for(++i; i < max; ++i)
			iov.at(i) =
			{
				const_cast<char *>(data(bufs[i])), size(bufs[i])
			};
	}

	const const_iovec_view ret{iov.data(), i};
	assert(bytes(ret) <= buffer::buffers::size(bufs));
	return ret;
}

size_t
ircd::fs::bytes(const const_iovec_view &iov)
{
	return std::accumulate(begin(iov), end(iov), size_t(0), []
	(auto ret, const auto &iov)
	{
		return ret += iov.iov_len;
	});
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/error.h
//

std::error_code
ircd::make_error_code(const boost::filesystem::filesystem_error &e)
{
	const boost::system::error_code &ec
	{
		e.code()
	};

	return make_error_code(ec);
}

//
// error::error
//

ircd::fs::error::error(const boost::filesystem::filesystem_error &code)
:std::system_error
{
    make_error_code(code)
}
{
	const string_view &msg
	{
		// Strip this prefix off the message to simplify it for our user.
		lstrip(code.what(), "boost::filesystem::")
	};

	copy(this->buf, msg);
}

ircd::fs::error::error(const std::system_error &code)
:std::system_error{code}
{
	string(this->buf, code);
}

ircd::fs::error::error(const std::error_code &code)
:std::system_error{code}
{
	string(this->buf, code);
}

const char *
ircd::fs::error::what()
const noexcept
{
	return this->ircd::error::what();
}
