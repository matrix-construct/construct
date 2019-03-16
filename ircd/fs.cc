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
#include <RB_INC_SYS_STATFS_H
#include <RB_INC_SYS_STATVFS_H
#include <boost/filesystem.hpp>
#include <RB_INC_SYS_SYSMACROS_H
#include <ircd/asio.h>

#ifdef IRCD_USE_AIO
	#include "fs_aio.h"
#endif

namespace ircd::fs
{
	static uint posix_flags(const std::ios::openmode &mode);
	static const char *path_str(const string_view &);
	static void debug_paths();
}

//
// init
//

ircd::fs::init::init()
:_aio_{}
{
	debug_paths();
}

ircd::fs::init::~init()
noexcept
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fs.h / misc
//

bool
ircd::fs::mkdir(const string_view &path)
try
{
	return filesystem::create_directories(_path(path));
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

bool
ircd::fs::remove(const string_view &path)
try
{
	return filesystem::remove(_path(path));
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
	return filesystem::remove(_path(path), ec);
}

bool
ircd::fs::rename(const string_view &old,
                 const string_view &new_)
try
{
	filesystem::rename(_path(old), _path(new_));
	return true;
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
	filesystem::rename(_path(old), _path(new_), ec);
	return !ec;
}

std::vector<std::string>
ircd::fs::ls_r(const string_view &path)
try
{
	const filesystem::recursive_directory_iterator end;
	filesystem::recursive_directory_iterator it
	{
		_path(path)
	};

	std::vector<std::string> ret;
	std::for_each(it, end, [&ret]
	(const auto &ent)
	{
		ret.emplace_back(ent.path().string());
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
		_path(path)
	};

	std::vector<std::string> ret;
	std::for_each(it, end, [&ret]
	(const auto &ent)
	{
		ret.emplace_back(ent.path().string());
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
	return filesystem::file_size(_path(path));
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

bool
ircd::fs::is_reg(const string_view &path)
try
{
	return filesystem::is_regular_file(_path(path));
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

bool
ircd::fs::is_dir(const string_view &path)
try
{
	return filesystem::is_directory(_path(path));
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

bool
ircd::fs::exists(const string_view &path)
try
{
	return filesystem::exists(_path(path));
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
	(ctx::ctx *const &interruptor)
	{
		fd.cancel();
	}};

	size_t len; continuation
	{
		continuation::asio_predicate, interruption, [&len, &fd, &sb]
		(auto &yield)
		{
			len = boost::asio::async_read_until(fd, sb, '\n', yield);
		}
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
		opts.metadata? aio::support_fdsync : aio::support_fsync
	};

	#ifdef IRCD_USE_AIO
	if(aio::system && opts.aio)
	{
		if(!opts.metadata && aio::support_fdsync)
			return aio::fdsync(fd, opts);

		if(aio::support_fsync)
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
	if(likely(aio::system) && opts.aio)
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

namespace ircd::fs
{
	static int flags(const read_opts &opts);
	static size_t _read(const fd &, const const_iovec_view &, const read_opts &);
	static size_t read(const fd &, const const_iovec_view &, const read_opts &);
}

/// Read from file descriptor fd into buffers. The number of bytes read into
/// the buffers is returned. By default (via read_opts.all) this call will
/// loop internally until the buffers are full or EOF. To allow for a partial
/// read(), disable read_opts.all. Note that to maintain alignments (i.e when
/// direct-io or for special files read_opts.all must be false). By default
/// (via read_opts.interruptible) this call can throw if the syscall was
/// interrupted before reading any bytes.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
size_t
__attribute__((stack_protect))
ircd::fs::read(const fd &fd,
               const mutable_buffers &bufs,
               const read_opts &opts_)
{
	if(unlikely(bufs.size() > info::iov_max))
		throw error
		{
			make_error_code(std::errc::invalid_argument),
			"Buffer count of %zu exceeds IOV_MAX of %zu",
			bufs.size(),
			info::iov_max
		};

	size_t ret(0);
	read_opts opts(opts_);
	assert(bufs.size() <= info::iov_max);
	struct ::iovec iovbuf[bufs.size()]; do
	{
		assert(opts.offset >= opts_.offset);
		const size_t off(opts.offset - opts_.offset);
		assert(off <= buffers::size(bufs));
		assert(ret <= buffers::size(bufs));
		const auto iov
		{
			make_iov({iovbuf, bufs.size()}, bufs, ret)
		};

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
#pragma GCC diagnostic pop

/// Lowest-level'ish read() call. This call only conducts a single operation
/// (no looping) and can return a partial read(). It does have branches
/// for various read_opts. The arguments involve `struct ::iovec` which
/// we do not expose to the ircd.h API; thus this function is internal to
/// ircd::fs. There is no reason to use this function in lieu of the public
/// fs::read() suite.
size_t
ircd::fs::read(const fd &fd,
               const const_iovec_view &iov,
               const read_opts &opts)
{
	#ifdef IRCD_USE_AIO
	if(aio::system && opts.aio)
		return aio::read(fd, iov, opts);
	#endif

	return _read(fd, iov, opts);
}

#ifdef HAVE_PREADV2
size_t
ircd::fs::_read(const fd &fd,
                const const_iovec_view &iov,
                const read_opts &opts)
{
	const auto &flags_
	{
		flags(opts)
	};

	const auto ret
	{
		opts.interruptible?
			syscall(::preadv2, fd, iov.data(), iov.size(), opts.offset, flags_):
			syscall_nointr(::preadv2, fd, iov.data(), iov.size(), opts.offset, flags_)
	};

	return size_t(ret);
}
#else
size_t
ircd::fs::_read(const fd &fd,
                const const_iovec_view &iov,
                const read_opts &opts)
{
	const auto ret
	{
		opts.interruptible?
			syscall(::preadv, fd, iov.data(), iov.size(), opts.offset):
			syscall_nointr(::preadv, fd, iov.data(), iov.size(), opts.offset)
	};

	return size_t(ret);
}
#endif // HAVE_PREADV2

int
ircd::fs::flags(const read_opts &opts)
{
	int ret{0};

	#if defined(RWF_HIPRI)
	if(aio::support_hipri && reqprio(opts.priority) == reqprio(opts::highest_priority))
		ret |= RWF_HIPRI;
	#endif

	#if defined(RWF_NOWAIT)
	if(aio::support_nowait && !opts.blocking)
		ret |= RWF_NOWAIT;
	#endif

	return ret;
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

// When we have pwritev2() we can use RWF_APPEND indicated by
// the -1. Otherwise, we don't keep flags in userspace and we
// don't check the fd for whether it was opened with O_APPEND
// so the user may just have to eat the cost of an extra lseek().
#ifdef HAVE_PWRITEV2
size_t
ircd::fs::append(const fd &fd,
                 const const_buffers &bufs,
                 const write_opts &opts_)
{
	auto opts(opts_);

	#if defined(RWF_APPEND)
	if(aio::support_append)
		opts.offset = -1;
	#endif

	if(!opts.offset)
		opts.offset = syscall(::lseek, fd, 0, SEEK_END);

	return write(fd, bufs, opts);
}
#else
size_t
ircd::fs::append(const fd &fd,
                 const const_buffers &bufs,
                 const write_opts &opts_)
{
	auto opts(opts_);
	if(!opts.offset || opts.offset == -1)
		opts.offset = syscall(::lseek, fd, 0, SEEK_END);

	return write(fd, bufs, opts);
}
#endif // HAVE_PWRITEV2

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

namespace ircd::fs
{
	static int flags(const write_opts &opts);
	static size_t _write(const fd &, const const_iovec_view &, const write_opts &);
	static size_t write(const fd &, const const_iovec_view &, const write_opts &);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
size_t
__attribute__((stack_protect))
ircd::fs::write(const fd &fd,
                const const_buffers &bufs,
                const write_opts &opts_)
{
	if(unlikely(bufs.size() > info::iov_max))
		throw error
		{
			make_error_code(std::errc::invalid_argument),
			"Buffer count of %zu exceeds IOV_MAX of %zu",
			bufs.size(),
			info::iov_max
		};

	size_t off(0);
	write_opts opts(opts_);
	assert(bufs.size() <= info::iov_max);
	struct ::iovec iovbuf[bufs.size()]; do
	{
		const auto iov
		{
			make_iov({iovbuf, bufs.size()}, bufs, off)
		};

		opts.offset += write(fd, iov, opts);
		assert(opts.offset >= opts_.offset);
		off = opts.offset - opts_.offset;
	}
	while(opts.all && opts_.offset >= 0 && off < buffers::size(bufs));
	assert(opts.offset >= opts_.offset);
	assert(ssize_t(off) == opts.offset - opts_.offset);
	assert(!opts.all || off == buffers::size(bufs));
	return off;
}
#pragma GCC diagnostic pop

/// Lowest-level'ish write() call. This call only conducts a single operation
/// (no looping) and can return early with a partial write(). It does have
/// branches for various write_opts. The arguments involve `struct ::iovec`
/// which we do not expose to the ircd.h API; thus this function is internal to
/// ircd::fs. There is no reason to use this function in lieu of the public
/// fs::read() suite.
size_t
ircd::fs::write(const fd &fd,
                const const_iovec_view &iov,
                const write_opts &opts)
{
	#ifdef IRCD_USE_AIO
	if(likely(aio::system) && opts.aio)
		return aio::write(fd, iov, opts);
	#endif

	return _write(fd, iov, opts);
}

#ifdef HAVE_PWRITEV2
size_t
ircd::fs::_write(const fd &fd,
                 const const_iovec_view &iov,
                 const write_opts &opts)
{
	// Manpages sez that when appending with RWF_APPEND, the offset has no
	// effect on the write; but if the value of the offset is -1 then the
	// fd's offset is updated, otherwise it is not.
	const off_t &offset
	{
		opts.offset == -1 && !opts.update_offset? 0 : opts.offset
	};

	const auto &flags_
	{
		flags(opts)
	};

	return
		opts.interruptible?
			syscall(::pwritev2, fd, iov.data(), iov.size(), offset, flags_):
			syscall_nointr(::pwritev2, fd, iov.data(), iov.size(), offset, flags_);
}
#else
size_t
ircd::fs::_write(const fd &fd,
                 const const_iovec_view &iov,
                 const write_opts &opts)
{
	return
		opts.interruptible?
			syscall(::pwritev, fd, iov.data(), iov.size(), opts.offset):
			syscall_nointr(::pwritev, fd, iov.data(), iov.size(), opts.offset);
}
#endif

int
ircd::fs::flags(const write_opts &opts)
{
	int ret{0};

	#if defined(RWF_APPEND)
	assert(opts.offset >= 0 || aio::support_append);
	if(aio::support_append && opts.offset == -1)
		ret |= RWF_APPEND;
	#endif

	#if defined(RWF_HIPRI)
	if(aio::support_hipri && reqprio(opts.priority) == reqprio(opts::highest_priority))
		ret |= RWF_HIPRI;
	#endif

	#if defined(RWF_NOWAIT)
	if(aio::support_nowait && !opts.blocking)
		ret |= RWF_NOWAIT;
	#endif

	#if defined(RWF_DSYNC)
	if(aio::support_dsync && opts.sync && !opts.metadata)
		ret |= RWF_DSYNC;
	#endif

	#if defined(RWF_SYNC)
	if(aio::support_sync && opts.sync && opts.metadata)
		ret |= RWF_SYNC;
	#endif

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/aio.h
//

//
// These symbols can be overriden by ircd/aio.cc if it is compiled and linked;
// otherwise on non-supporting platforms these will be the defaults here.
//

decltype(ircd::fs::aio::support)
extern __attribute__((weak))
ircd::fs::aio::support;

decltype(ircd::fs::aio::support_sync)
extern __attribute__((weak))
ircd::fs::aio::support_sync
{
	info::kversion[0] >= 4 &&
	info::kversion[1] >= 7
};

decltype(ircd::fs::aio::support_dsync)
extern __attribute__((weak))
ircd::fs::aio::support_dsync
{
	info::kversion[0] >= 4 &&
	info::kversion[1] >= 7
};

decltype(ircd::fs::aio::support_hipri)
extern __attribute__((weak))
ircd::fs::aio::support_hipri
{
	info::kversion[0] >= 4 &&
	info::kversion[1] >= 6
};

decltype(ircd::fs::aio::support_nowait)
extern __attribute__((weak))
ircd::fs::aio::support_nowait
{
	info::kversion[0] >= 4 &&
	info::kversion[1] >= 14
};

decltype(ircd::fs::aio::support_append)
extern __attribute__((weak))
ircd::fs::aio::support_append
{
	info::kversion[0] >= 4 &&
	info::kversion[1] >= 16
};

decltype(ircd::fs::aio::support_fsync)
extern __attribute__((weak))
ircd::fs::aio::support_fsync
{
	info::kversion[0] >= 4 &&
	info::kversion[1] >= 18
};

decltype(ircd::fs::aio::support_fdsync)
extern __attribute__((weak))
ircd::fs::aio::support_fdsync
{
	info::kversion[0] >= 4 &&
	info::kversion[1] >= 18
};

decltype(ircd::fs::aio::MAX_EVENTS)
extern __attribute__((weak))
ircd::fs::aio::MAX_EVENTS;

decltype(ircd::fs::aio::MAX_REQPRIO)
extern __attribute__((weak))
ircd::fs::aio::MAX_REQPRIO
{
	20
};

/// Conf item to control whether AIO is enabled or bypassed.
decltype(ircd::fs::aio::enable)
ircd::fs::aio::enable
{
	{ "name",     "ircd.fs.aio.enable"  },
	{ "default",  true                  },
	{ "persist",  false                 },
};

decltype(ircd::fs::aio::max_events)
ircd::fs::aio::max_events
{
	{ "name",     "ircd.fs.aio.max_events"  },
	{ "default",  long(aio::MAX_EVENTS)     },
	{ "persist",  false                     },
};

decltype(ircd::fs::aio::max_submit)
ircd::fs::aio::max_submit
{
	{ "name",     "ircd.fs.aio.max_submit"  },
	{ "default",  32L                       },
	{ "persist",  false                     },
};

/// Global stats structure
decltype(ircd::fs::aio::stats)
ircd::fs::aio::stats;

/// Non-null when aio is available for use
decltype(ircd::fs::aio::system)
ircd::fs::aio::system;

//
// init
//

#ifndef IRCD_USE_AIO
ircd::fs::aio::init::init()
{
	assert(!context);
	log::warning
	{
		"No support for asynchronous local filesystem IO..."
	};
}
#endif

#ifndef IRCD_USE_AIO
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

namespace ircd::fs
{
	static long pathconf(const fd &, const int &arg);
}

decltype(ircd::fs::fd::opts::direct_io_enable)
ircd::fs::fd::opts::direct_io_enable
{
	{ "name",     "ircd.fs.fd.direct_io_enable"  },
	{ "default",  true                           },
	{ "persist",  false                          },
};

#ifdef HAVE_SYS_STAT_H
ulong
ircd::fs::device(const fd &fd)
{
	struct stat st{0};
	syscall(::fstat, fd, &st);
	return st.st_dev;
}
#else
ulong
ircd::fs::device(const fd &fd)
{
	static_assert
	(
		0, "Please implement this definition"
	)
}
#endif

#ifdef HAVE_SYS_STATFS_H
ulong
ircd::fs::fstype(const fd &fd)
{
	struct statfs f{0};
	syscall(::fstatfs, fd, &f);
	return f.f_type;
}
#else
ulong
ircd::fs::fstype(const fd &fd)
{
	static_assert
	(
		0, "Please implement this definition"
	)
}
#endif

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

long
ircd::fs::pathconf(const fd &fd,
                   const int &arg)
{
	return syscall(::fpathconf, fd, arg);
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
	const ctx::slice_usage_warning message
	{
		"fs::fs::fd(): open(2): %s", path
	};

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
// fs/device.h
//

#ifdef __linux__
ircd::string_view
ircd::fs::dev::sysfs(const mutable_buffer &out,
                     const ulong &id,
                     const string_view &relpath)
{
	const string_view path{fmt::sprintf
	{
		path_scratch, "/sys/dev/block/%s/%s",
		sysfs_id(name_scratch, id),
		relpath
	}};

	fs::read_opts opts;
	opts.aio = false;
	return fs::read(path, out, opts);
}
#else
ircd::string_view
ircd::fs::dev::sysfs(const mutable_buffer &out,
                     const ulong &id,
                     const string_view &relpath)
{
	throw panic
	{
		"sysfs(5) is not available."
	};
}
#endif

ircd::string_view
ircd::fs::dev::sysfs_id(const mutable_buffer &out,
                        const ulong &id)
{
	return sysfs_id(out, dev::id(id));
}

ircd::string_view
ircd::fs::dev::sysfs_id(const mutable_buffer &out,
                        const major_minor &id)
{
	return fmt::sprintf
	{
		out, "%lu:%lu", id.first, id.second
	};
}

ulong
ircd::fs::dev::id(const major_minor &id)
{
	return gnu_dev_makedev(id.first, id.second);
}

ircd::fs::dev::major_minor
ircd::fs::dev::id(const ulong &id)
{
	return
	{
		gnu_dev_major(id), gnu_dev_minor(id)
	};
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/opts.h
//

decltype(ircd::fs::opts_default)
ircd::fs::opts_default
{};

decltype(ircd::fs::opts::highest_priority)
ircd::fs::opts::highest_priority
{
	std::numeric_limits<decltype(priority)>::min()
};

///////////////////////////////////////////////////////////////////////////////
//
// fs/iov.h
//

ircd::fs::const_iovec_view
ircd::fs::make_iov(const iovec_view &iov,
                   const mutable_buffers &bufs,
                   const size_t &offset)
{
	assert(offset <= buffers::size(bufs));
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
	assert(offset <= buffers::size(bufs));
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
// fs/path.h
//

/// Default maximum path string length (for all filesystems & platforms).
decltype(ircd::fs::NAME_MAX_LEN)
ircd::fs::NAME_MAX_LEN
{
	#ifdef NAME_MAX
		NAME_MAX
	#elif defined(_POSIX_NAME_MAX)
		_POSIX_NAME_MAX
	#else
		255
	#endif
};

/// Default maximum path string length (for all filesystems & platforms).
decltype(ircd::fs::PATH_MAX_LEN)
ircd::fs::PATH_MAX_LEN
{
	#ifdef PATH_MAX
		PATH_MAX
	#elif defined(_POSIX_PATH_MAX)
		_POSIX_PATH_MAX
	#else
		4096
	#endif
};

namespace ircd::fs
{
	thread_local char _path_scratch[PATH_MAX_LEN];
	thread_local char _name_scratch[NAME_MAX_LEN];
}

decltype(ircd::fs::path_scratch)
ircd::fs::path_scratch
{
	_path_scratch
};

decltype(ircd::fs::name_scratch)
ircd::fs::name_scratch
{
	_name_scratch
};

std::string
ircd::fs::cwd()
try
{
	const auto &cur
	{
		filesystem::current_path()
	};

	return cur.string();
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

ircd::string_view
ircd::fs::cwd(const mutable_buffer &buf)
try
{
	const auto &cur
	{
		filesystem::current_path()
	};

	return strlcpy(buf, cur.native());
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

#ifdef _PC_PATH_MAX
size_t
ircd::fs::path_max_len(const string_view &path)
{
	return pathconf(path, _PC_PATH_MAX);
}
#else
size_t
ircd::fs::path_max_len(const string_view &path)
{
	return PATH_MAX_LEN;
}
#endif

#ifdef _PC_NAME_MAX
size_t
ircd::fs::name_max_len(const string_view &path)
{
	return pathconf(path, _PC_NAME_MAX);
}
#elif defined(HAVE_SYS_STATFS_H)
size_t
ircd::fs::name_max_len(const string_view &path)
{
	struct statfs f{0};
	syscall(::statfs, path_str(path), &f);
	return f.f_namelen;
}
#else
size_t
ircd::fs::name_max_len(const string_view &path)
{
	return NAME_MAX_LEN;
}
#endif

long
ircd::fs::pathconf(const string_view &path,
                   const int &arg)
{
	return syscall(::pathconf, path_str(path), arg);
}

ircd::string_view
ircd::fs::filename(const mutable_buffer &buf,
                   const string_view &p)
{
	return path(buf, _path(p).filename());
}

ircd::string_view
ircd::fs::extension(const mutable_buffer &buf,
                    const string_view &p)
{
	return path(buf, _path(p).extension());
}

ircd::string_view
ircd::fs::extension(const mutable_buffer &buf,
                    const string_view &p,
                    const string_view &replace)
{
	return path(buf, _path(p).replace_extension(_path(replace)));
}

ircd::string_view
ircd::fs::relative(const mutable_buffer &buf,
                   const string_view &root,
                   const string_view &p)
{
	return path(buf, relative(_path(p), _path(root)));
}

bool
ircd::fs::is_relative(const string_view &p)
{
	return _path(p).is_relative();
}

bool
ircd::fs::is_absolute(const string_view &p)
{
	return _path(p).is_absolute();
}

//
// fs::path()
//

ircd::string_view
ircd::fs::path(const mutable_buffer &buf,
               const filesystem::path &path)
{
	return strlcpy(buf, path.c_str());
}

ircd::string_view
ircd::fs::path(const mutable_buffer &buf,
               const vector_view<const std::string> &list)
{
	return strlcpy(buf, _path(list).c_str());
}

ircd::string_view
ircd::fs::path(const mutable_buffer &buf,
               const vector_view<const string_view> &list)
{
	return strlcpy(buf, _path(list).c_str());
}

ircd::string_view
ircd::fs::path(const mutable_buffer &buf,
               const base &base,
               const string_view &rest)
{
	const auto p
	{
		_path(std::initializer_list<const string_view>
		{
			path(base),
			rest,
		})
	};

	return strlcpy(buf, p.c_str());
}

ircd::string_view
ircd::fs::path(const base &base)
noexcept
{
	return basepath::get(base).path;
}

//
// fs::_path()
//

boost::filesystem::path
ircd::fs::_path(const vector_view<const std::string> &list)
try
{
	filesystem::path ret;
	for(const auto &s : list)
		ret /= s;

	return ret.string();
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

boost::filesystem::path
ircd::fs::_path(const vector_view<const string_view> &list)
try
{
	filesystem::path ret;
	for(const auto &s : list)
		ret /= _path(s);

	return ret.string();
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

boost::filesystem::path
ircd::fs::_path(const string_view &s)
try
{
	return _path(std::string{s});
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

boost::filesystem::path
ircd::fs::_path(std::string s)
try
{
	return filesystem::path{std::move(s)};
}
catch(const filesystem::filesystem_error &e)
{
	throw error{e};
}

//
// fs::basepath
//

namespace ircd::fs
{
	extern const std::array<basepath, num_of<base>()> basepaths;
}

decltype(ircd::fs::basepaths)
ircd::fs::basepaths
{{
	{ "installation prefix",      RB_PREFIX      },
	{ "binary directory",         RB_BIN_DIR     },
	{ "configuration directory",  RB_CONF_DIR    },
	{ "data directory",           RB_DATA_DIR    },
	{ "database directory",       RB_DB_DIR      },
	{ "log directory",            RB_LOG_DIR     },
	{ "module directory",         RB_MODULE_DIR  },
}};

const ircd::fs::basepath &
ircd::fs::basepath::get(const base &base)
noexcept
{
	return basepaths.at(base);
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

///////////////////////////////////////////////////////////////////////////////
//
// Internal utils
//

void
ircd::fs::debug_paths()
{
	thread_local char buf[PATH_MAX_LEN + 1];
	log::debug
	{
		"Current working directory: `%s'", cwd(buf)
	};

	for_each<base>([](const base &base)
	{
		log::debug
		{
			"Working %s is `%s'",
			basepath::get(base).name,
			basepath::get(base).path,
		};
	});
}

const char *
ircd::fs::path_str(const string_view &s)
{
	static const size_t buf_sz{PATH_MAX_LEN}, buf_cnt{4};
	thread_local char buf[buf_cnt][buf_sz];
	thread_local size_t i{0};

	return data(strlcpy(buf[i++ % buf_cnt], s));
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

/// Translate an ircd::fs opts priority integer to an AIO priority integer.
/// The ircd::fs priority integer is like a nice value. The AIO value is
/// positive [0, MAX_REQPRIO]. This function takes an ircd::fs value and
/// shifts it to the AIO value.
int
ircd::fs::reqprio(int input)
{
	const auto &max_reqprio
	{
		aio::MAX_REQPRIO
	};

	static const auto median
	{
		int(max_reqprio / 2)
	};

	input = std::max(input, 0 - median);
	input = std::min(input, median);
	input = max_reqprio - (input + median);
	assert(input >= 0 && input <= int(max_reqprio));
	return input;
}
