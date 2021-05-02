// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_FCNTL_H
#include <RB_INC_SYS_STAT_H
#include <RB_INC_SYS_STATFS_H
#include <RB_INC_SYS_STATVFS_H
#include <RB_INC_SYS_RESOURCE_H
#include <boost/filesystem.hpp>

#ifdef IRCD_USE_AIO
	#include "fs_aio.h"
#endif

#ifdef IRCD_USE_IOU
	#include "fs_iou.h"
#endif

// TODO: prevents use until io_uring support implemented
#undef IRCD_USE_IOU

namespace ircd::fs
{
	extern conf::item<ulong> rlimit_nofile;

	static void update_rlimit_nofile();
	static void init_dump_info();
}

decltype(ircd::fs::log)
ircd::fs::log
{
	"fs"
};

decltype(ircd::fs::rlimit_nofile)
ircd::fs::rlimit_nofile
{
	{
		{ "name",      "ircd.fs.rlimit.nofile"  },
		{ "default",   65535L                   },
		{ "persist",   false                    },
	},
	update_rlimit_nofile
};

//
// init::init
//

ircd::fs::init::init()
{
	init_dump_info();
}

ircd::fs::init::~init()
noexcept
{
}

void
ircd::fs::init_dump_info()
{
	const bool support_async
	{
		false || iou::system || aio::system
	};

	if(!support_async)
		log::warning
		{
			log, "Support for asynchronous filesystem IO has not been"
			" established. Filesystem IO is degraded to synchronous system calls."
		};
}

#if defined(HAVE_SYS_RESOURCE_H) && defined(RLIMIT_NOFILE)
void
ircd::fs::update_rlimit_nofile()
try
{
	rlimit rlim[2];
	syscall(getrlimit, RLIMIT_NOFILE, &rlim[0]);

	rlim[1] = rlim[0];
	rlim[1].rlim_cur = std::max(ulong(rlim[1].rlim_cur), ulong(fs::rlimit_nofile));
	rlim[1].rlim_cur = std::min(rlim[1].rlim_cur, rlim[1].rlim_max);
	if(rlim[0].rlim_cur == rlim[1].rlim_cur)
		return;

	syscall(setrlimit, RLIMIT_NOFILE, &rlim[1]);
	log::info
	{
		log, "Raised resource limit for number of open files from %ld to %ld",
		rlim[0].rlim_cur,
		rlim[1].rlim_cur,
	};
}
catch(const std::system_error &e)
{
	log::warning
	{
		log, "Failed to raise resource limit for number of open files :%s",
		e.what()
	};
}
#else
void
ircd::fs::init_rlimit_nofile()
{
	log::dwarning
	{
		log, "Cannot modify resource limit for number of open files."
	};
}
#endif

///////////////////////////////////////////////////////////////////////////////
//
// fs/support.h
//

decltype(ircd::fs::support::pwritev2)
ircd::fs::support::pwritev2
{
	#if defined(HAVE_PWRITEV2)
		info::kernel_version[0] > 4 ||
		(info::kernel_version[0] >= 4 && info::kernel_version[1] >= 6)
	#else
		false
	#endif
};

decltype(ircd::fs::support::preadv2)
ircd::fs::support::preadv2
{
	#if defined(HAVE_PREADV2)
		info::kernel_version[0] > 4 ||
		(info::kernel_version[0] >= 4 && info::kernel_version[1] >= 6)
	#else
		false
	#endif
};

decltype(ircd::fs::support::sync)
ircd::fs::support::sync
{
	#if defined(HAVE_PWRITEV2) && defined(RWF_SYNC)
		info::kernel_version[0] > 4 ||
		(info::kernel_version[0] >= 4 && info::kernel_version[1] >= 7)
	#else
		false
	#endif
};

decltype(ircd::fs::support::dsync)
ircd::fs::support::dsync
{
	#if defined(HAVE_PWRITEV2) && defined(RWF_DSYNC)
		info::kernel_version[0] > 4 ||
		(info::kernel_version[0] >= 4 && info::kernel_version[1] >= 7)
	#else
		false
	#endif
};

decltype(ircd::fs::support::hipri)
ircd::fs::support::hipri
{
	#if defined(HAVE_PWRITEV2) && defined(RWF_HIPRI)
		info::kernel_version[0] > 4 ||
		(info::kernel_version[0] >= 4 && info::kernel_version[1] >= 6)
	#else
		false
	#endif
};

decltype(ircd::fs::support::nowait)
ircd::fs::support::nowait
{
	#if defined(HAVE_PWRITEV2) && defined(RWF_NOWAIT)
		info::kernel_version[0] > 4 ||
		(info::kernel_version[0] >= 4 && info::kernel_version[1] >= 14)
	#else
		false
	#endif
};

decltype(ircd::fs::support::append)
ircd::fs::support::append
{
	#if defined(HAVE_PWRITEV2) && defined(RWF_APPEND)
		info::kernel_version[0] > 4 ||
		(info::kernel_version[0] >= 4 && info::kernel_version[1] >= 16)
	#else
		false
	#endif
};

decltype(ircd::fs::support::rwh_write_life)
ircd::fs::support::rwh_write_life
{
	#if defined(HAVE_FCNTL_H) && defined(F_SET_FILE_RW_HINT)
		info::kernel_version[0] > 4 ||
		(info::kernel_version[0] >= 4 && info::kernel_version[1] >= 13)
	#else
		false
	#endif
};

decltype(ircd::fs::support::rwf_write_life)
ircd::fs::support::rwf_write_life
{
	#if defined(RWF_WRITE_LIFE_SHIFT)
		false //TODO: XXX
	#else
		false
	#endif
};

decltype(ircd::fs::support::aio)
ircd::fs::support::aio
{
	#ifdef IRCD_USE_AIO
		true
	#else
		false
	#endif
};

void
ircd::fs::support::dump_info()
{
	#if defined(IRCD_USE_AIO) || defined(IRCD_USE_IOU)
		const bool support_async {true};
	#else
		const bool support_async {false};
	#endif

	char support[128] {0};
	const auto _append{[&support]
	(const string_view &name, const bool &avail, const int &enable)
	{
		strlcat(support, fmt::bsprintf<64>
		{
			"%s:%c%s ",
			name,
			avail == true? 'y': 'n',
			enable == true? "y": enable == false? "n": "",
		});
	}};

	_append("async", support_async, -1);
	_append("preadv2", preadv2, -1);
	_append("pwritev2", pwritev2, -1);
	_append("SYNC", sync, -1);
	_append("DSYNC", dsync, -1);
	_append("HIPRI", hipri, -1);
	_append("NOWAIT", nowait, -1);
	_append("APPEND", append, -1);
	_append("RWH", rwh_write_life, -1);
	_append("RWF", rwf_write_life, -1);

	log::info
	{
		log, "VFS %s",
		support
	};

	#ifdef RB_DEBUG
	const unique_mutable_buffer buf
	{
		PATH_MAX_LEN + 1
	};

	log::debug
	{
		log, "Current working directory: `%s'", cwd(buf)
	};
	#endif
}

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

#if defined(HAVE_SYS_RESOURCE_H) && defined(RLIMIT_FSIZE)
size_t
ircd::fs::support::rlimit_fsize()
{
	rlimit rlim;
	syscall(getrlimit, RLIMIT_FSIZE, &rlim);
	return rlim.rlim_cur;
}
#else
size_t
ircd::fs::support::rlimit_fize()
{
	return -1;
}
#endif

#if defined(HAVE_SYS_RESOURCE_H) && defined(RLIMIT_NOFILE)
size_t
ircd::fs::support::rlimit_nofile()
{
	rlimit rlim;
	syscall(getrlimit, RLIMIT_NOFILE, &rlim);
	return rlim.rlim_cur;
}
#else
size_t
ircd::fs::support::rlimit_nofile()
{
	return -1;
}
#endif

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
	const prof::syscall_usage_warning message
	{
		"fs::remove(%s)", path
	};

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
	const prof::syscall_usage_warning message
	{
		"fs::remove(%s)", path
	};

	boost::system::error_code ec;
	return filesystem::remove(_path(path), ec);
}

bool
ircd::fs::rename(const string_view &old,
                 const string_view &new_)
try
{
	const prof::syscall_usage_warning message
	{
		"fs::rename(%s, %s)", old, new_
	};

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
	const prof::syscall_usage_warning message
	{
		"fs::rename(%s, %s)", old, new_
	};

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
ircd::fs::is_exec(const string_view &path)
try
{
	return filesystem::status(_path(path)).permissions() & filesystem::owner_exe;
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
	throw_system_error(e.code());
	__builtin_unreachable();
}

//
// tty
//

ircd::fs::stdin::tty::tty()
:fd{[]
{
	char buf[256];
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
// fs/select.h
//

size_t
ircd::fs::select(const vector_view<const fd> &fd_)
{
	using asio::posix::stream_descriptor;

	static ios::descriptor desc
	{
		"ircd::fs::select"
	};

	const size_t num(size(fd_));
	std::optional<stream_descriptor> _fd[num];
	const unwind release{[&_fd]
	{
		for(auto &fd : _fd)
			if(fd)
				fd->release();
	}};

	size_t ret(-1);
	ctx::latch latch(num);
	const auto callback{[&num, &_fd, &latch, &ret]
	(const boost::system::error_code &ec, const auto &fd)
	{
		// The first successful callback is associated with an input fd
		// and its array indice becomes the return value.
		if(!ec && ret == size_t(-1))
		{
			const auto it
			{
				std::find_if(_fd, _fd + num, [&fd]
				(const auto &_fd)
				{
					return _fd && std::addressof(*_fd) == std::addressof(*fd);
				})
			};

			ret = std::distance(_fd, it);
			assert(ret < num);
		}

		latch.count_down();
	}};

	for(size_t i(0); i < num; ++i)
	{
		// Allow a closed descriptor in the vector to be no-op.
		if(!fd_[i])
		{
			latch.count_down();
			continue;
		}

		_fd[i] =
		{
			ios::get(), int(fd_[i])
		};

		auto handle
		{
			std::bind(callback, ph::_1, std::cref(_fd[i]))
		};

		_fd[i]->async_wait(stream_descriptor::wait_read, ios::handle(desc, std::move(handle)));
	}

	std::exception_ptr eptr; try
	{
		latch.wait();
		assert(ret < num);
		return ret;
	}
	catch(...)
	{
		eptr = std::current_exception();
		const ctx::exception_handler eh;
		const ctx::uninterruptible::nothrow ui;
		for(auto &fd : _fd)
			fd->cancel();

		latch.wait();
		assert(eptr);
		std::rethrow_exception(eptr);
	}

	return ret;
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
	assert(opts.op == op::SYNC);
	const prof::syscall_usage_warning message
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
	assert(opts.op == op::SYNC);

	#ifdef IRCD_USE_IOU
	if(iou::system && opts.aio)
		return void(iou::fsync(fd, opts));
	#endif

	#ifdef IRCD_USE_AIO
	if(aio::system && opts.aio)
	{
		if(support::aio_fdsync && !opts.metadata)
			return void(aio::fsync(fd, opts));

		else if(support::aio_fsync && opts.metadata)
			return void(aio::fsync(fd, opts));
	}
	#endif

	const prof::syscall_usage_warning message
	{
		"fs::flush(fd:%d, {metadata:%b aio:%b:%b})",
		int(fd),
		opts.metadata,
		opts.aio,
		opts.metadata? support::aio_fsync : support::aio_fdsync
	};

	if(!opts.metadata)
		return void(syscall(::fdatasync, fd));

	return void(syscall(::fsync, fd));
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/read.h
//

ircd::fs::read_opts
const ircd::fs::read_opts_default
{};

size_t
ircd::fs::prefetch(const fd &fd,
                   const size_t &count,
                   const read_opts &opts)
{
	#if defined(POSIX_FADV_WILLNEED)
		return advise(fd, POSIX_FADV_WILLNEED, count, opts);
	#else
		return 0UL;
	#endif
}

bool
ircd::fs::incore(const fd &fd,
                 const size_t &count,
                 const read_opts &opts)
{
	fs::map::opts map_opts;
	map_opts.offset = buffer::align(opts.offset, info::page_size);
	map_opts.blocking = false;
	const size_t &map_size
	{
		count?: size(fd)
	};

	const size_t &map_pages
	{
		(map_size + info::page_size - 1) / info::page_size
	};

	assert(map_opts.offset % 4096 == 0);
	const fs::map map
	{
		fd, map_opts, map_size
	};

	const size_t res
	{
		allocator::incore(map)
	};

	return res == map_size;
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
ircd::fs::read(const vector_view<read_op> &op)
{
	// Use IOV_MAX as a sanity value for number of operations here
	if(unlikely(op.size() > info::iov_max))
		throw error
		{
			make_error_code(std::errc::invalid_argument),
			"Read operation count:%zu exceeds max:%zu",
			op.size(),
			info::iov_max,
		};

	bool aio {true}, all {false};
	for(size_t i(0); i < op.size(); ++i)
	{
		assert(op[i].opts);
		assert(op[i].opts->aio);

		// If any op isn't tolerant of less bytes actually read than they
		// requested, they require us to perform the unix read loop, and
		// that ruins things for everybody!
		assert(!op[i].opts->all);
		//all |= op[i].opts->all;

		// If any op doesn't want AIO we have to fallback on sequential
		// blocking reads for all ops.
		assert(op[i].opts->aio);
		//aio &= op[i].opts->aio;

		// EINVAL for exceeding this system's IOV_MAX
		if(unlikely(op[i].bufs.size() > info::iov_max))
			throw error
			{
				make_error_code(std::errc::invalid_argument),
				"op[%zu] :buffer count of %zu exceeds IOV_MAX of %zu",
				i,
				op[i].bufs.size(),
				info::iov_max,
			};
	}

	#ifdef IRCD_USE_AIO
	if(likely(aio::system && aio && !all))
		return aio::read(op);
	#endif

	// Fallback to sequential read operations
	size_t ret(0);
	for(size_t i(0); i < op.size(); ++i) try
	{
		assert(op[i].fd);
		assert(op[i].opts);
		op[i].ret = read(*op[i].fd, op[i].bufs, *op[i].opts);
		ret += op[i].ret;
	}
	catch(const std::system_error &)
	{
		op[i].eptr = std::current_exception();
		op[i].ret = 0;
	}

	return ret;
}

namespace ircd::fs
{
	static int flags(const read_opts &opts);
	static size_t _read_preadv2(const fd &, const const_iovec_view &, const read_opts &);
	static size_t _read_preadv(const fd &, const const_iovec_view &, const read_opts &);
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

		const size_t last
		{
			read(fd, iov, opts)
		};

		if(!opts_.blocking && !last)
			break;

		ret += last;
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
	assert(opts.op == op::READ);

	#ifdef IRCD_USE_IOU
	if(likely(iou::system && opts.aio))
		return iou::read(fd, iov, opts);
	#endif

	#ifdef IRCD_USE_AIO
	if(likely(aio::system && opts.aio))
		return aio::read(fd, iov, opts);
	#endif

	#ifdef HAVE_PREADV2
	return support::preadv2?
		_read_preadv2(fd, iov, opts):
		_read_preadv(fd, iov, opts);
	#else
	return _read_preadv(fd, iov, opts);
	#endif
}

size_t
ircd::fs::_read_preadv(const fd &fd,
                       const const_iovec_view &iov,
                       const read_opts &opts)
{
	ssize_t ret; do
	{
		ret = ::preadv(int(fd), iov.data(), iov.size(), opts.offset);
	}
	while(!opts.interruptible && unlikely(ret == -1 && errno == EINTR));

	static_assert(EAGAIN == EWOULDBLOCK);
	if(unlikely(!opts.blocking && ret == -1 && errno == EAGAIN))
		return 0UL;

	if(unlikely(ret == -1))
		throw std::system_error
		{
			errno, std::system_category()
		};

	return ret;
}

#ifdef HAVE_PREADV2
size_t
ircd::fs::_read_preadv2(const fd &fd,
                        const const_iovec_view &iov,
                        const read_opts &opts)
{
	const auto &flags_
	{
		flags(opts)
	};

	ssize_t ret; do
	{
		ret = ::preadv2(int(fd), iov.data(), iov.size(), opts.offset, flags_);
	}
	while(!opts.interruptible && unlikely(ret == -1 && errno == EINTR));

	static_assert(EAGAIN == EWOULDBLOCK);
	if(!opts.blocking && ret == -1 && errno == EAGAIN)
		return 0UL;

	if(unlikely(ret == -1))
		throw std::system_error
		{
			errno, std::system_category()
		};

	return ret;
}
#endif HAVE_PREADV2

int
ircd::fs::flags(const read_opts &opts)
{
	int ret{0};

	#if defined(RWF_HIPRI)
	if(support::hipri && reqprio(opts.priority) == reqprio(opts::highest_priority))
		ret |= RWF_HIPRI;
	#endif

	#if defined(RWF_NOWAIT)
	if(support::nowait && !opts.blocking)
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
	assert(opts.op == op::WRITE);

	int mode{0};

	#ifdef FALLOC_FL_KEEP_SIZE
	mode |= opts.keep_size? FALLOC_FL_KEEP_SIZE : 0;
	#else
	if(opts.keep_size)
		throw_system_error(std::errc::invalid_argument);
	#endif

	#ifdef FALLOC_FL_PUNCH_HOLE
	mode |= opts.punch_hole? FALLOC_FL_PUNCH_HOLE : 0;
	#else
	if(opts.punch_hole)
		throw_system_error(std::errc::invalid_argument);
	#endif

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
	assert(opts.op == op::WRITE);
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

//
// append
//

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
	if(support::pwritev2 && support::append)
		opts.offset = -1;
	else if(!opts.offset || opts.offset == -1)
		opts.offset = syscall(::lseek, fd, 0, SEEK_END);

	return write(fd, bufs, opts);
}

//
// write
//

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
	static size_t _write_pwritev2(const fd &, const const_iovec_view &, const write_opts &);
	static size_t _write_pwritev(const fd &, const const_iovec_view &, const write_opts &);
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

		const size_t last
		{
			write(fd, iov, opts)
		};

		opts.offset += last;
		assert(opts.offset >= opts_.offset);
		off = opts.offset - opts_.offset;
		if(!opts.blocking && !last)
			break;
	}
	while(opts.all && opts_.offset >= 0 && off < buffers::size(bufs));
	assert(opts.offset >= opts_.offset);
	assert(ssize_t(off) == opts.offset - opts_.offset);
	assert(!opts.all || !opts.blocking || off == buffers::size(bufs));
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
	assert(opts.op == op::WRITE);

	#ifdef IRCD_USE_IOU
	if(likely(iou::system && opts.aio))
		return iou::write(fd, iov, opts);
	#endif

	#ifdef IRCD_USE_AIO
	if(likely(aio::system && opts.aio))
		return aio::write(fd, iov, opts);
	#endif

	#ifdef HAVE_PWRITEV2
	return support::pwritev2?
		_write_pwritev2(fd, iov, opts):
		_write_pwritev(fd, iov, opts);
	#else
	return _write_pwritev(fd, iov, opts);
	#endif
}

size_t
ircd::fs::_write_pwritev(const fd &fd,
                         const const_iovec_view &iov,
                         const write_opts &opts)
{
	ssize_t ret; do
	{
		ret = ::pwritev(int(fd), iov.data(), iov.size(), opts.offset);
	}
	while(!opts.interruptible && unlikely(ret == -1 && errno == EINTR));

	static_assert(EAGAIN == EWOULDBLOCK);
	if(unlikely(!opts.blocking && ret == -1 && errno == EAGAIN))
		return 0UL;

	if(unlikely(ret == -1))
		throw std::system_error
		{
			errno, std::system_category()
		};

	return ret;
}

#ifdef HAVE_PWRITEV2
size_t
ircd::fs::_write_pwritev2(const fd &fd,
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

	ssize_t ret; do
	{
		ret = ::pwritev2(int(fd), iov.data(), iov.size(), opts.offset, flags(opts));
	}
	while(!opts.interruptible && unlikely(ret == -1 && errno == EINTR));

	static_assert(EAGAIN == EWOULDBLOCK);
	if(!opts.blocking && ret == -1 && errno == EAGAIN)
		return 0UL;

	if(unlikely(ret == -1))
		throw std::system_error
		{
			errno, std::system_category()
		};

	return ret;
}
#endif HAVE_PWRITEV2

int
ircd::fs::flags(const write_opts &opts)
{
	int ret{0};

	#if defined(RWF_APPEND)
	assert(opts.offset >= 0 || support::append);
	if(support::append && opts.offset == -1)
		ret |= RWF_APPEND;
	#endif

	#if defined(RWF_HIPRI)
	if(support::hipri && reqprio(opts.priority) == reqprio(opts::highest_priority))
		ret |= RWF_HIPRI;
	#endif

	#if defined(RWF_NOWAIT)
	if(support::nowait && !opts.blocking)
		ret |= RWF_NOWAIT;
	#endif

	#if defined(RWF_DSYNC)
	if(support::dsync && opts.sync && !opts.metadata)
		ret |= RWF_DSYNC;
	#endif

	#if defined(RWF_SYNC)
	if(support::sync && opts.sync && opts.metadata)
		ret |= RWF_SYNC;
	#endif

	#ifdef RWF_WRITE_LIFE_SHIFT
	if(support::rwf_write_life && opts.write_life)
		ret |= (opts.write_life << (RWF_WRITE_LIFE_SHIFT));
	#endif

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/wait.h
//

namespace ircd::fs
{
	static constexpr asio::posix::stream_descriptor::wait_type translate(const ready &) noexcept
	__attribute__((const));
}

decltype(ircd::fs::wait_opts_default)
ircd::fs::wait_opts_default;

void
ircd::fs::wait(const fd &fd,
               const wait_opts &opts)
{
	assert(opts.op == op::WAIT);

	const auto &wait_type
	{
		translate(opts.ready)
	};

	boost::asio::posix::stream_descriptor sd
	{
		ios::get(), int(fd)
	};

	const unwind release{[&sd]
	{
		sd.release();
	}};

	const auto interruption{[&sd]
	(ctx::ctx *const &interruptor)
	{
		sd.cancel();
	}};

	boost::system::error_code ec; continuation
	{
		continuation::asio_predicate, interruption, [&wait_type, &sd, &ec]
		(auto &yield)
		{
			sd.async_wait(wait_type, yield[ec]);
		}
	};

	if(unlikely(ec))
		throw_system_error(ec);
}

constexpr
boost::asio::posix::stream_descriptor::wait_type
ircd::fs::translate(const ready &ready)
noexcept
{
	using wait_type = boost::asio::posix::stream_descriptor::wait_type;

	switch(ready)
	{
		case ready::ANY:
			return wait_type::wait_read | wait_type::wait_write | wait_type::wait_error;

		case ready::READ:
			return wait_type::wait_read;

		case ready::WRITE:
			return wait_type::wait_write;

		case ready::ERROR:
		default:
			return wait_type::wait_error;
	}
}

ircd::string_view
ircd::fs::reflect(const ready &ready)
{
	switch(ready)
	{
		case ready::ANY:      return "ANY";
		case ready::READ:     return "READ";
		case ready::WRITE:    return "WRITE";
		case ready::ERROR:    return "ERROR";
	}

	return "?????";
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/aio.h
//

decltype(ircd::fs::aio::MAX_EVENTS)
ircd::fs::aio::MAX_EVENTS
{
	info::aio_max
};

decltype(ircd::fs::aio::MAX_REQPRIO)
ircd::fs::aio::MAX_REQPRIO
{
	info::aio_reqprio_max
};

/// Conf item to control whether aio is enabled or bypassed.
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
decltype(ircd::fs::aio::system)
ircd::fs::aio::system;

//
// init
//

#ifndef IRCD_USE_AIO
[[gnu::weak]]
ircd::fs::aio::init::init()
{
	assert(!system);
}
#endif

#ifndef IRCD_USE_AIO
[[gnu::weak]]
ircd::fs::aio::init::~init()
noexcept
{
	assert(!system);
}
#endif

//
// stats
//

ircd::fs::aio::stats::stats()
:value{0}
,items{0}
,requests
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.requests" },
	}
}
,complete
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.complete" },
	}
}
,submits
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.submits" },
	}
}
,chases
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.chases" },
	}
}
,handles
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.handles" },
	}
}
,events
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.events" },
	}
}
,cancel
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.cancel" },
	}
}
,errors
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.errors" },
	}
}
,reads
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.reads" },
	}
}
,writes
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.writes" },
	}
}
,stalls
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.stalls" },
	}
}
,bytes_requests
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.bytes.requests" },
	}
}
,bytes_complete
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.bytes.complete" },
	}
}
,bytes_errors
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.bytes.errors" },
	}
}
,bytes_cancel
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.bytes.cancel" },
	}
}
,bytes_read
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.bytes.read" },
	}
}
,bytes_write
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.bytes.write" },
	}
}
,cur_bytes_write
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.cur.bytes.write" },
	}
}
,cur_reads
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.cur.reads" },
	}
}
,cur_writes
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.cur.writes" },
	}
}
,cur_queued
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.cur.queued" },
	}
}
,cur_submits
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.cur.submits" },
	}
}
,max_requests
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.max.requests" },
	}
}
,max_reads
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.max.reads" },
	}
}
,max_writes
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.max.writes" },
	}
}
,max_queued
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.max.queued" },
	}
}
,max_submits
{
	value + items++,
	{
		{ "name", "ircd.fs.aio.max.submits" },
	}
}
{
	assert(items <= (sizeof(value) / sizeof(value[0])));
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/iou.h
//

decltype(ircd::fs::iou::support)
ircd::fs::iou::support
{
	#ifdef IRCD_USE_IOU
		info::kernel_version[0] > 5 ||
		(info::kernel_version[0] >= 5 && info::kernel_version[1] >= 1)
	#else
		false
	#endif
};

/// Conf item to control whether iou is enabled or bypassed.
decltype(ircd::fs::iou::enable)
ircd::fs::iou::enable
{
	{ "name",     "ircd.fs.iou.enable"  },
	{ "default",  false                 },
	{ "persist",  false                 },
};

/// Global stats structure
decltype(ircd::fs::iou::stats)
ircd::fs::iou::stats
{
	fs::aio::stats
};

/// Non-null when iou is available for use
decltype(ircd::fs::iou::system)
ircd::fs::iou::system;

//
// init
//

#ifndef IRCD_USE_IOU
[[gnu::weak]]
ircd::fs::iou::init::init()
{
	assert(!system);
}
#endif

#ifndef IRCD_USE_IOU
[[gnu::weak]]
ircd::fs::iou::init::~init()
noexcept
{
	assert(!system);
}
#endif

///////////////////////////////////////////////////////////////////////////////
//
// fs/map.h
//

namespace ircd::fs
{
	static uint flags(const map::opts &);
	static uint prot(const map::opts &);
}

size_t
ircd::fs::evict(const map &map,
                const size_t &len,
                const opts &opts)
{
	const size_t offset
	{
		buffer::align(opts.offset, info::page_size)
	};

	const mutable_buffer buf
	{
		map + offset, len
	};

	return allocator::evict(buf);
}

size_t
ircd::fs::prefetch(const map &map,
                   const size_t &len,
                   const opts &opts)
{
	const size_t offset
	{
		buffer::align(opts.offset, info::page_size)
	};

	const mutable_buffer buf
	{
		map + offset, len
	};

	return allocator::prefetch(buf);
}

size_t
ircd::fs::advise(const map &map,
                 const int &advice,
                 const size_t &len,
                 const opts &opts)
{
	const mutable_buffer buf
	{
		map + opts.offset, len
	};

	return allocator::advise(buf, advice);
}

//
// map::map
//

decltype(ircd::fs::map::default_opts)
ircd::fs::map::default_opts;

ircd::fs::map::map(const fd &fd,
                   const opts &opts,
                   const size_t &size)
{
	const auto map_size
	{
		size?: fs::size(fd)
	};

	#ifdef HAVE_MMAP
	void *const &ptr
	{
		::mmap(nullptr, map_size, prot(opts), flags(opts), int(fd), opts.offset)
	};
	#else
	#error "Missing ::mmap(2) on this platform."
	#endif

	if(unlikely(ptr == MAP_FAILED))
		throw_system_error(errno);

	static_cast<mutable_buffer &>(*this) = mutable_buffer
	{
		reinterpret_cast<char *>(ptr),
		map_size
	};

	const int advise
	{
		#if defined(HAVE_POSIX_MADVISE)
		opts.random?      POSIX_MADV_RANDOM:
		opts.sequential?  POSIX_MADV_SEQUENTIAL:
		opts.dontneed?    POSIX_MADV_DONTNEED:
		#endif
		0
	};

	if(advise)
		fs::advise(*this, advise, map_size);
}

ircd::fs::map::~map()
noexcept try
{
	if(mutable_buffer::null())
		return;

	syscall(::munmap, data(*this), size(*this));
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "munmap(%p, %zu) :%s",
		data(static_cast<mutable_buffer &>(*this)),
		size(static_cast<mutable_buffer &>(*this)),
		e.what(),
	};
}

ircd::fs::map &
ircd::fs::map::operator=(map &&other)
noexcept
{
	auto &ours
	{
		static_cast<mutable_buffer &>(*this)
	};

	auto &theirs
	{
		static_cast<mutable_buffer &>(other)
	};

	this->~map();
	ours = theirs;
	theirs = {};
	return *this;
}

//
// util
//

uint
ircd::fs::prot(const map::opts &opts)
{
	uint ret
	{
		PROT_NONE
	};

	if(opts.mode & std::ios::in)
		ret |= PROT_READ;

	if(opts.mode & std::ios::out)
		ret |= PROT_WRITE;

	assert(!opts.execute);
	if((false) && opts.execute)
		ret |= PROT_EXEC;

	return ret;
}

uint
ircd::fs::flags(const map::opts &opts)
{
	uint ret
	{
		0
	};

	if(opts.shared)
		ret |= MAP_SHARED;
	else
		ret |= MAP_PRIVATE;

	#if defined(MAP_NONBLOCK)
	if(!opts.blocking)
		ret |= MAP_NONBLOCK;
	#endif

	#if defined(MAP_POPULATE)
	if(opts.populate)
		ret |= MAP_POPULATE;
	#endif

	#if defined(MAP_NORESERVE)
	if(!opts.reserve)
		ret |= MAP_NORESERVE;
	#endif

	#if defined(MAP_LOCKED)
	if(opts.locked)
		ret |= MAP_LOCKED;
	#endif

	#if defined(MAP_HUGE_TLB) && defined(MAP_HUGE_2MB)
	if(opts.huge2mb)
		ret |= MAP_HUGETLB | MAP_HUGE_2MB;
	#elif defined(MAP_HUGE_SHIFT)
	if(opts.huge2mb)
		ret |= (21 << MAP_HUGE_SHIFT);
	#elif defined(MAP_HUGETLB)
	if(opts.huge2mb)
		ret |= MAP_HUGE_TLB
	#else
		#warning "MAP_HUGETLB (2MB) not supported"
	#endif

	#if defined(MAP_HUGE_TLB) && defined(MAP_HUGE_1GB)
	if(opts.huge1gb)
		ret |= MAP_HUGE_1GB;
	#elif defined(MAP_HUGE_SHIFT)
	if(opts.huge1gb)
		ret |= (30 << MAP_HUGE_SHIFT);
	#elif defined(MAP_HUGE_TLB)
	if(opts.huge1gb)
		ret |= MAP_HUGE_TLB
	#else
		#warning "MAP_HUGETLB (1GB) not supported"
	#endif

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/fd.h
//

namespace ircd::fs
{
	static uint flags(const fd::opts &);
	static uint flags(const std::ios::openmode &);
	static long pathconf(const fd &, const int &arg);
}

decltype(ircd::fs::fd::opts::direct_io_enable)
ircd::fs::fd::opts::direct_io_enable
{
	{ "name",     "ircd.fs.fd.direct_io_enable"  },
	{ "default",  true                           },
	{ "persist",  false                          },
};

#if defined(POSIX_FADV_DONTNEED)
size_t
ircd::fs::evict(const fd &fd,
                const size_t &count,
                const opts &opts)
{
	return advise(fd, POSIX_FADV_DONTNEED, count, opts);
}
#else
#warning "POSIX_FADV_DONTNEED not available on this platform."
size_t
ircd::fs::evict(const fd &fd,
                const size_t &count,
                const opts &opts)
{
	return 0UL;
}
#endif

#if defined(HAVE_POSIX_FADVISE)
size_t
ircd::fs::advise(const fd &fd,
                 const int &advice,
                 const size_t &count,
                 const opts &opts)
{
	static const size_t max_count
	{
		128_KiB
	};

	size_t i(0), off, cnt; do
	{
		off = opts.offset + max_count * i++;
		cnt = std::min(opts.offset + count - off, max_count);
		switch(const auto r(::posix_fadvise(fd, off, cnt, advice)); r)
		{
			case 0:   break;
			default:  throw_system_error(r);
		}
	}
	while(off + cnt < opts.offset + count);
	return count;
}
#else
#warning "posix_fadvise(2) not available for this compilation."
size_t
ircd::fs::advise(const fd &fd,
                 const int &advice,
                 const size_t &count,
                 const opts &opts)
{
	return 0UL;
}
#endif

#if defined(HAVE_FCNTL_H) && defined(F_SET_FILE_RW_HINT)
void
ircd::fs::write_life(const fd &fd,
                     const uint64_t &hint)
{
	if(!support::rwh_write_life)
		return;

	syscall(::fcntl, int(fd), F_SET_FILE_RW_HINT, &hint);
}
#else
#warning "F_SET_FILE_RW_HINT not supported on platform."
void
ircd::fs::write_life(const fd &fd,
                     const uint64_t &hint)
{
}
#endif

#if defined(HAVE_FCNTL_H) && defined(F_GET_FILE_RW_HINT)
uint64_t
ircd::fs::write_life(const fd &fd)
noexcept try
{
	uint64_t ret;
	syscall(::fcntl, int(fd), F_GET_FILE_RW_HINT, &ret);
	return ret;
}
catch(const std::system_error &e)
{
	log::derror
	{
		log, "fcntl(F_GET_FILE_RW_HINT) fd:%d :%s",
		int(fd),
		e.what()
	};

	return 0;
}
#else
#warning "F_GET_FILE_RW_HINT not supported on platform."
uint64_t
ircd::fs::write_life(const fd &fd)
{
	return 0UL;
}
#endif

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
	fs::flags(mode)
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

ircd::fs::fd::fd(const int &fdno)
:fdno
{
	fdno
}
{
}

ircd::fs::fd::fd(const string_view &path)
:fd
{
	path, opts{}
}
{
}

ircd::fs::fd::fd(const string_view &path,
                 const opts &opts)
:fd
{
	AT_FDCWD, path, opts
}
{
}

ircd::fs::fd::fd(const int &dirfd,
                 const string_view &path,
                 const opts &opts)
try
:fdno
{
	-1 // sentinel value for inert dtor
}
{
	const unwind_exceptional dtor_on_error
	{
		[this] { this->~fd(); }
	};

	const mode_t mode
	{
		mode_t(opts.mask)
	};

	const uint &flags
	{
		fs::flags(opts)
	};

	const int &advise
	{
		opts.direct?
			0:
		opts.random?
			POSIX_FADV_RANDOM:
		opts.sequential?
			POSIX_FADV_SEQUENTIAL:
		opts.dontneed?
			POSIX_FADV_DONTNEED:
			0
	};

	{
		const prof::syscall_usage_warning message
		{
			"fs::fs::fd(): openat(2): %s", path
		};

		assert((flags & ~O_CREAT) || mode != 0);
		fdno = syscall(::openat, dirfd, path_cstr(path), flags, mode);
	}

	if(advise)
		fs::advise(*this, advise);

	if(opts.ate)
		syscall(::lseek, fdno, 0, SEEK_END);
}
catch(const std::system_error &e)
{
	if(opts.errlog)
		log::derror
		{
			log, "`%s' :%s",
			path,
			e.what(),
		};

	throw;
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
noexcept
{
	if(likely(fdno >= 0)) try
	{
		syscall(::close, fdno);
	}
	catch(const std::exception &e)
	{
		log::critical
		{
			"Failed to close fd:%d :%s",
			fdno,
			e.what()
		};
	}
}

int
ircd::fs::fd::release()
noexcept
{
	const int fdno(this->fdno);
	this->fdno = -1;
	return fdno;
}

ircd::fs::fd::opts
ircd::fs::fd::options()
const
{
	opts ret;
	ret.flags = syscall(::fcntl, int(*this), F_GETFL, 0);

	if((ret.flags & O_RDONLY) == O_RDONLY)
		ret.mode = std::ios::in;

	if((ret.flags & O_WRONLY) == O_WRONLY)
		ret.mode = std::ios::out;

	if((ret.flags & O_RDWR) == O_RDWR)
		ret.mode = std::ios::in | std::ios::out;

	if((ret.flags & O_TRUNC) == O_TRUNC)
		ret.mode = std::ios::trunc;

	ret.direct = ret.flags & O_DIRECT;
	ret.cloexec = ret.flags & O_CLOEXEC;
	ret.create = ret.flags & O_CREAT;
	ret.blocking = ret.flags & O_NONBLOCK;
	ret.exclusive = ret.flags & O_EXCL;
	return ret;
}

uint
ircd::fs::flags(const fd::opts &opts)
{
	uint ret(opts.flags);
	ret |= fs::flags(opts.mode);
	ret |= opts.direct? O_DIRECT : 0UL;
	ret |= opts.cloexec? O_CLOEXEC : 0UL;
	ret |= opts.create? O_CREAT : 0UL;
	ret |= !opts.blocking? O_NONBLOCK : 0UL;
	ret |= opts.exclusive? O_EXCL : 0UL;
	return ret;
}

uint
ircd::fs::flags(const std::ios::openmode &mode)
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
// fs/op.h
//

ircd::string_view
ircd::fs::reflect(const op &op)
{
	switch(op)
	{
		case op::NOOP:    return "NOOP";
		case op::READ:    return "READ";
		case op::WRITE:   return "WRITE";
		case op::SYNC:    return "SYNC";
		case op::WAIT:    return "WAIT";
	}

	return "????";
}

#ifndef IRCD_USE_AIO
[[gnu::weak]]
ircd::fs::op
ircd::fs::aio::translate(const int &val)
{
	return op::NOOP;
}
#endif

#ifndef IRCD_USE_IOU
[[gnu::weak]]
ircd::fs::op
ircd::fs::iou::translate(const int &val)
{
	return op::NOOP;
}
#endif

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
		if(size(bufs[i]) >= offset - off)
		{
			assert(offset >= off);
			off = offset - off;
			break;
		}

	assert(i <= max);
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

	assert(i <= max);
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
		if(size(bufs[i]) >= offset - off)
		{
			assert(offset >= off);
			off = offset - off;
			break;
		}

	assert(i <= max);
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

	assert(i <= max);
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

std::string
ircd::string(const boost::filesystem::filesystem_error &e)
{
	return ircd::string(512, [&e]
	(const mutable_buffer &buf)
	{
		return string(buf, e);
	});
}

ircd::string_view
ircd::string(const mutable_buffer &buf,
             const boost::filesystem::filesystem_error &e)
{
	return fmt::sprintf
	{
		buf, "%s :%s", e.code().category().name(), e.what()
	};
}

std::system_error
ircd::make_system_error(const boost::filesystem::filesystem_error &e)
{
	return std::system_error
	{
		make_error_code(e), e.what()
	};
}

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

decltype(ircd::fs::error::buf)
thread_local
ircd::fs::error::buf;

ircd::fs::error::error(const boost::filesystem::filesystem_error &e)
:std::system_error
{
	make_system_error(e)
}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// Internal utils
//

/// Translate an ircd::fs opts priority integer to an AIO priority integer.
/// The ircd::fs priority integer is like a nice value. The AIO value is
/// positive [0, MAX_REQPRIO]. This function takes an ircd::fs value and
/// shifts it to the AIO value.
int
ircd::fs::reqprio(int input)
noexcept
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
