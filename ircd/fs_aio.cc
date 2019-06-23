// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <sys/syscall.h>
#include <sys/eventfd.h>
#include <ircd/asio.h>
#include "fs_aio.h"

///////////////////////////////////////////////////////////////////////////////
//
// ircd/fs/aio.h
//
// The contents of this section override weak symbols in ircd/fs.cc when this
// unit is conditionally compiled and linked on AIO-supporting platforms. On
// non-supporting platforms, or for items not listed here, the definitions in
// ircd/fs.cc are the default.

decltype(ircd::fs::aio::support)
ircd::fs::aio::support
{
	true
};

/// True if IOCB_CMD_FSYNC is supported by AIO. If this is false then
/// fs::fsync_opts::async=true flag is ignored.
decltype(ircd::fs::aio::support_fsync)
ircd::fs::aio::support_fsync
{
	#if defined(RWF_SYNC)
		info::kernel_version[0] >= 4 &&
		info::kernel_version[1] >= 18
	#else
		false
	#endif
};

/// True if IOCB_CMD_FDSYNC is supported by AIO. If this is false then
/// fs::fsync_opts::async=true flag is ignored.
decltype(ircd::fs::aio::support_fdsync)
ircd::fs::aio::support_fdsync
{
	#if defined(RWF_DSYNC)
		info::kernel_version[0] >= 4 &&
		info::kernel_version[1] >= 18
	#else
		false
	#endif
};

decltype(ircd::fs::aio::MAX_EVENTS)
ircd::fs::aio::MAX_EVENTS
{
	128L //TODO: get this info
};

decltype(ircd::fs::aio::MAX_REQPRIO)
ircd::fs::aio::MAX_REQPRIO
{
	info::aio_reqprio_max
};

//
// init
//

ircd::fs::aio::init::init()
{
	assert(!system);
	if(!bool(aio::enable))
		return;

	system = new struct aio::system
	(
		size_t(max_events),
		size_t(max_submit)
	);
}

ircd::fs::aio::init::~init()
noexcept
{
	delete system;
	system = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/fs/op.h
//
// The contents of this section override weak symbols in ircd/fs.cc when this
// unit is conditionally compiled and linked on AIO-supporting platforms.

ircd::fs::op
ircd::fs::aio::translate(const int &val)
{
	switch(val)
	{
		case IOCB_CMD_PREAD:     return op::READ;
		case IOCB_CMD_PWRITE:    return op::WRITE;
		case IOCB_CMD_FSYNC:     return op::SYNC;
		case IOCB_CMD_FDSYNC:    return op::SYNC;
		case IOCB_CMD_NOOP:      return op::NOOP;
		case IOCB_CMD_PREADV:    return op::READ;
		case IOCB_CMD_PWRITEV:   return op::WRITE;
	}

	return op::NOOP;
}

///////////////////////////////////////////////////////////////////////////////
//
// fs_aio.h
//

//
// request::fsync
//

ircd::fs::aio::request::fsync::fsync(const int &fd,
                                     const sync_opts &opts)
:request{fd, &opts}
{
	assert(opts.op == op::SYNC);
	aio_lio_opcode = IOCB_CMD_FSYNC;

	aio_buf = 0;
	aio_nbytes = 0;
	aio_offset = 0;
}

void
ircd::fs::aio::fsync(const fd &fd,
                     const sync_opts &opts)
{
	aio::request::fsync request
	{
		fd, opts
	};

	request();
}

//
// request::fdsync
//

ircd::fs::aio::request::fdsync::fdsync(const int &fd,
                                       const sync_opts &opts)
:request{fd, &opts}
{
	assert(opts.op == op::SYNC);
	aio_lio_opcode = IOCB_CMD_FDSYNC;

	aio_buf = 0;
	aio_nbytes = 0;
	aio_offset = 0;
}

void
ircd::fs::aio::fdsync(const fd &fd,
                      const sync_opts &opts)
{
	aio::request::fdsync request
	{
		fd, opts
	};

	request();
}

//
// request::read
//

ircd::fs::aio::request::read::read(const int &fd,
                                   const const_iovec_view &iov,
                                   const read_opts &opts)
:request{fd, &opts}
{
	assert(opts.op == op::READ);
	aio_lio_opcode = IOCB_CMD_PREADV;

	aio_buf = uintptr_t(iov.data());
	aio_nbytes = iov.size();
	aio_offset = opts.offset;
}

size_t
ircd::fs::aio::read(const fd &fd,
                    const const_iovec_view &bufs,
                    const read_opts &opts)
{
	aio::request::read request
	{
		fd, bufs, opts
	};

	const scope_count cur_reads{stats.cur_reads};
	stats.max_reads = std::max(stats.max_reads, stats.cur_reads);

	#if defined(RWF_NOWAIT) && defined(RB_DEBUG_FS_AIO_READ_BLOCKING)
	request.aio_rw_flags |= support_nowait? RWF_NOWAIT : 0;
	#endif

	size_t bytes
	{
		request()
	};

	#if defined(RWF_NOWAIT) && defined(RB_DEBUG_FS_AIO_READ_BLOCKING)
	const bool would_block
	{
		opts.blocking &&
		request.aio_rw_flags & RWF_NOWAIT &&
		request.retval == -1 &&
		request.errcode == EAGAIN
	};

	if(would_block)
	{
		log::dwarning
		{
			log, "read blocks io_submit(): fd:%d size:%zu off:%zd op:%u pri:%u in_flight:%zu qcount:%zu",
			request.aio_fildes,
			request.aio_nbytes,
			request.aio_offset,
			request.aio_lio_opcode,
			request.aio_reqprio,
			system->in_flight,
			system->qcount,
		};

		assert(bytes == 0);
		request.aio_rw_flags &= ~RWF_NOWAIT;
		request.retval = -2;
		request.errcode = 0;
		bytes = request();
	}
	#endif

	stats.bytes_read += bytes;
	stats.reads++;
	return bytes;
}

//
// request::write
//

ircd::fs::aio::request::write::write(const int &fd,
                                     const const_iovec_view &iov,
                                     const write_opts &opts)
:request{fd, &opts}
{
	assert(opts.op == op::WRITE);
	aio_lio_opcode = IOCB_CMD_PWRITEV;

	aio_buf = uintptr_t(iov.data());
	aio_nbytes = iov.size();
	aio_offset = opts.offset;

	#if defined(RWF_APPEND)
	if(support_append && opts.offset == -1)
	{
		// AIO departs from pwritev2() behavior and EINVAL's on -1.
		aio_offset = 0;
		aio_rw_flags |= RWF_APPEND;
	}
	#endif

	#if defined(RWF_DSYNC)
	if(support_dsync && opts.sync && !opts.metadata)
		aio_rw_flags |= RWF_DSYNC;
	#endif

	#if defined(RWF_SYNC)
	if(support_sync && opts.sync && opts.metadata)
		aio_rw_flags |= RWF_SYNC;
	#endif

	#ifdef RWF_WRITE_LIFE_SHIFT
	if(support_rwf_write_life && opts.write_life)
		aio_rw_flags |= (opts.write_life << (RWF_WRITE_LIFE_SHIFT));
	#endif
}

size_t
ircd::fs::aio::write(const fd &fd,
                     const const_iovec_view &bufs,
                     const write_opts &opts)
{
	aio::request::write request
	{
		fd, bufs, opts
	};

	const size_t req_bytes
	{
		fs::bytes(request.iovec())
	};

	// track current write count
	const scope_count cur_writes{stats.cur_writes};
	stats.max_writes = std::max(stats.max_writes, stats.cur_writes);

	// track current write bytes count
	stats.cur_bytes_write += req_bytes;
	const unwind dec{[&req_bytes]
	{
		stats.cur_bytes_write -= req_bytes;
	}};

	// Make the request; ircd::ctx blocks here. Throws on error
	const size_t bytes
	{
		request()
	};

	// Does linux ever not complete all bytes for an AIO?
	assert(!opts.blocking || bytes == req_bytes);

	stats.bytes_write += bytes;
	stats.writes++;
	return bytes;
}

size_t
ircd::fs::aio::count_queued(const op &type)
{
	assert(system);
	const auto &qcount(system->qcount);
	return std::count_if(begin(system->queue), begin(system->queue)+qcount, [&type]
	(const iocb *const &iocb)
	{
		assert(iocb);
		return aio::translate(iocb->aio_lio_opcode) == type;
	});
}

bool
ircd::fs::aio::for_each_queued(const std::function<bool (const request &)> &closure)
{
	assert(system);
	for(size_t i(0); i < system->qcount; ++i)
		if(!closure(*reinterpret_cast<const request *>(system->queue[i]->aio_data)))
			return false;

	return true;
}

bool
ircd::fs::aio::for_each_completed(const std::function<bool (const request &)> &closure)
{
	assert(system && system->head);
	const auto &max{system->head->nr};
	volatile auto head(system->head->head);
	volatile const auto &tail(system->head->tail);
	for(; head != tail; ++head, head %= max)
		if(!closure(*reinterpret_cast<const request *>(system->ring[head].data)))
			return false;

	return true;
}

//
// request
//

ircd::fs::aio::request::request(const int &fd,
                                const struct opts *const &opts)
:iocb{0}
,opts{opts}
{
	assert(system);
	assert(ctx::current);

	aio_flags = IOCB_FLAG_RESFD;
	aio_resfd = system->resfd.native_handle();
	aio_fildes = fd;
	aio_data = uintptr_t(this);
	aio_reqprio = reqprio(opts->priority);

	#if defined(RWF_HIPRI)
	if(support_hipri && aio_reqprio == reqprio(opts::highest_priority))
		aio_rw_flags |= RWF_HIPRI;
	#endif

	#if defined(RWF_NOWAIT)
	if(support_nowait && !opts->blocking)
		aio_rw_flags |= RWF_NOWAIT;
	#endif
}

ircd::fs::aio::request::~request()
noexcept
{
}

/// Cancel a request. The handler callstack is invoked directly from here
/// which means any callback will be invoked or ctx will be notified if
/// appropriate.
bool
ircd::fs::aio::request::cancel()
{
	assert(system);
	if(!system->cancel(*this))
		return false;

	stats.bytes_cancel += bytes(iovec());
	stats.cancel++;
	return true;
}

/// Submit a request and properly yield the ircd::ctx. When this returns the
/// result will be available or an exception will be thrown.
size_t
ircd::fs::aio::request::operator()()
{
	assert(system);
	assert(ctx::current);
	assert(waiter == ctx::current);

	const size_t submitted_bytes
	{
		bytes(iovec())
	};

	// Update stats for submission phase
	stats.bytes_requests += submitted_bytes;
	stats.requests++;

	const uint16_t &curcnt(stats.requests - stats.complete);
	stats.max_requests = std::max(stats.max_requests, curcnt);

	// Wait here until there's room to submit a request
	system->dock.wait([]
	{
		return system->request_avail() > 0;
	});

	// Submit to system
	system->submit(*this);

	// Wait for completion
	while(!system->wait(*this));

	assert(completed());
	assert(retval <= ssize_t(submitted_bytes));

	// Update stats for completion phase.
	stats.bytes_complete += submitted_bytes;
	stats.complete++;

	if(likely(retval != -1))
		return size_t(retval);

	assert(opts);
	const auto blocking
	{
		#if defined(RWF_NOWAIT)
			~aio_rw_flags & RWF_NOWAIT
		#else
			opts->blocking
		#endif
	};

	static_assert(EAGAIN == EWOULDBLOCK);
	if(!blocking && retval == -1 && errcode == EAGAIN)
		return 0UL;

	stats.errors++;
	stats.bytes_errors += submitted_bytes;
	thread_local char errbuf[512]; fmt::sprintf
	{
		errbuf, "fd:%d size:%zu off:%zd op:%u pri:%u #%lu",
		aio_fildes,
		aio_nbytes,
		aio_offset,
		aio_lio_opcode,
		aio_reqprio,
		errcode
	};

	throw std::system_error
	{
		make_error_code(errcode), errbuf
	};
}

bool
ircd::fs::aio::request::queued()
const
{
	return !for_each_queued([this]
	(const auto &request)
	{
		return &request != this; // true to continue and return true
	});
}

bool
ircd::fs::aio::request::completed()
const
{
	return retval >= -1L;
}

ircd::fs::const_iovec_view
ircd::fs::aio::request::iovec()
const
{
	return
	{
		reinterpret_cast<const ::iovec *>(aio_buf), aio_nbytes
	};
}

//
// system
//

decltype(ircd::fs::aio::system::eventfd_flags)
ircd::fs::aio::system::eventfd_flags
{
	EFD_CLOEXEC | EFD_NONBLOCK
};

//
// system::system
//

ircd::fs::aio::system::system(const size_t &max_events,
                              const size_t &max_submit)
try
:event
{
	max_events
}
,queue
{
	max_submit?: max_events
}
,resfd
{
	ios::get(), int(syscall(::eventfd, ecount, eventfd_flags))
}
,head
{
	[this]
	{
		aio_context *idp {nullptr};
		syscall<SYS_io_setup>(this->max_events(), &idp);
		return idp;
	}(),
	[](const aio_context *const &head)
	{
		syscall<SYS_io_destroy>(head);
	}
}
,ring
{
	reinterpret_cast<const io_event *>
	(
		reinterpret_cast<const uint8_t *>(head.get()) +
		sizeof(aio_context)
	)
}
{
	assert(head->magic == aio_context::MAGIC);
	if(unlikely(head->magic != aio_context::MAGIC))
		throw panic
		{
			"ircd::fs::aio kernel context structure magic:%u != %u",
			head->magic,
			aio_context::MAGIC,
		};

	assert(sizeof(aio_context) == head->header_length);
	if(unlikely(head->header_length != sizeof(*head)))
		throw panic
		{
			"ircd::fs::aio kernel context structure length:%u != %u",
			head->header_length,
			sizeof(*head),
		};

	log::debug
	{
		log, "Established head(%p) ring(%p) id:%u fd:%d max_events:%zu max_submit:%zu compat:%x incompat:%x len:%u nr:%u",
		head.get(),
		ring,
		head->id,
		int(resfd.native_handle()),
		this->max_events(),
		this->max_submit(),
		head->compat_features,
		head->incompat_features,
		head->header_length,
		head->nr
	};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Error starting AIO context %p :%s",
		(const void *)this,
		e.what()
	};
}

ircd::fs::aio::system::~system()
noexcept try
{
	assert(qcount == 0);
	const ctx::uninterruptible::nothrow ui;

	interrupt();
	wait();

	boost::system::error_code ec;
	resfd.close(ec);
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Error shutting down AIO context %p :%s",
		(const void *)this,
		e.what()
	};
}

bool
ircd::fs::aio::system::interrupt()
{
	if(!resfd.is_open())
		return false;

	if(handle_set)
		resfd.cancel();
	else
		ecount = -1;

	return true;
}

bool
ircd::fs::aio::system::wait()
{
	if(!resfd.is_open())
		return false;

	log::debug
	{
		log, "Waiting for AIO context %p", this
	};

	dock.wait([this]
	{
		return ecount == uint64_t(-1);
	});

	assert(request_count() == 0);
	return true;
}

/// Block the current context while waiting for results.
///
/// This function returns true when the request completes and it's safe to
/// continue. This function intercepts all exceptions and cancels the request
/// if it's appropriate before rethrowing; after which it is safe to continue.
///
/// If this function returns false it is not safe to continue; it *must* be
/// called again until it no longer returns false.
bool
ircd::fs::aio::system::wait(request &request)
try
{
	assert(ctx::current == request.waiter);
	while(!request.completed())
		ctx::wait();

	return true;
}
catch(...)
{
	// When the ctx is interrupted we're obliged to cancel the request
	// if it has not reached a completed state.
	if(request.completed())
		throw;

	// The handler callstack is invoked synchronously on this stack for
	// requests which are still in our userspace queue.
	if(request.queued())
	{
		request.cancel();
		throw;
	}

	// The handler callstack is invoked asynchronously for requests
	// submitted to the kernel; we *must* wait for that by blocking
	// ctx interrupts and terminations and continue to wait. The caller
	// must loop into this call again until it returns true or throws.
	return false;
}

bool
ircd::fs::aio::system::cancel(request &request)
try
{
	assert(request.aio_data == uintptr_t(&request));
	assert(!request.completed() || request.queued());

	iocb *const cb
	{
		static_cast<iocb *>(&request)
	};

	const auto eit
	{
		std::remove(begin(queue), begin(queue) + qcount, cb)
	};

	const auto qcount
	{
		size_t(std::distance(begin(queue), eit))
	};

	// We know something was erased if the qcount no longer matches
	const bool erased_from_queue
	{
		this->qcount > qcount
	};

	// Make the qcount accurate again after any erasure.
	assert(!erased_from_queue || this->qcount == qcount + 1);
	assert(erased_from_queue || this->qcount == qcount);
	if(erased_from_queue)
	{
		this->qcount--;
		dock.notify_one();
		stats.cur_queued--;
	}

	// Setup an io_event result which we will handle as a normal event
	// immediately on this stack. We create our own cancel result if
	// the request was not yet submitted to the system so the handler
	// remains agnostic to our userspace queues.
	io_event result {0};
	if(erased_from_queue)
	{
		result.data = cb->aio_data;
		result.obj = uintptr_t(cb);
		result.res = -1;
		result.res2 = ECANCELED;
	} else {
		assert(!request.queued());
		syscall_nointr<SYS_io_cancel>(head.get(), cb, &result);
		in_flight--;
		stats.cur_submits--;
		dock.notify_one();
	}

	handle_event(result);
	return true;
}
catch(const std::system_error &e)
{
	assert(request.aio_data == uintptr_t(&request));
	log::critical
	{
		"AIO(%p) cancel(fd:%d size:%zu off:%zd op:%u pri:%u) #%lu :%s",
		this,
		request.aio_fildes,
		request.aio_nbytes,
		request.aio_offset,
		request.aio_lio_opcode,
		request.aio_reqprio,
		e.code().value(),
		e.what()
	};

	return false;
}

bool
ircd::fs::aio::system::submit(request &request)
{
	assert(request.opts);
	assert(qcount < queue.size());
	assert(qcount + in_flight < max_events());
	assert(request.aio_data == uintptr_t(&request));
	assert(!request.completed());
	const ctx::critical_assertion ca;

	queue.at(qcount++) = static_cast<iocb *>(&request);
	stats.cur_queued++;
	stats.max_queued = std::max(stats.max_queued, stats.cur_queued);
	assert(stats.cur_queued == qcount);

	// Determine whether this request will trigger a flush of the queue
	// and be submitted itself as well.
	const bool submit_now
	{
		// The nodelay flag is set by the user.
		request.opts->nodelay

		// The queue has reached its limits.
		|| qcount >= max_submit()
	};

	const size_t submitted
	{
		submit_now? submit() : 0
	};

	// Only post the chaser when the queue has one item. If it has more
	// items the chaser was already posted after the first item and will
	// flush the whole queue down to 0.
	if(qcount == 1)
	{
		static ios::descriptor descriptor
		{
			"ircd::fs::aio chase"
		};

		auto handler(std::bind(&system::chase, this));
		ircd::defer(descriptor, std::move(handler));
	}

	return true;
}

/// The chaser is posted to the IRCd event loop after the first request.
/// Ideally more requests will queue up before the chaser reaches the front
/// of the IRCd event queue and executes.
void
ircd::fs::aio::system::chase()
noexcept try
{
	if(!qcount)
		return;

	const auto submitted
	{
		submit()
	};

	stats.chases++;
	assert(!qcount);
}
catch(const std::exception &e)
{
	throw panic
	{
		"AIO(%p) system::chase() qcount:%zu :%s", this, qcount, e.what()
	};
}

/// The submitter submits all queued requests and resets our userspace queue
/// count down to zero.
size_t
ircd::fs::aio::system::submit()
noexcept try
{
	assert(qcount > 0);
	assert(in_flight + qcount <= MAX_EVENTS);
	assert(in_flight + qcount <= max_events());
	const bool idle
	{
		in_flight == 0
	};

	size_t submitted; do
	{
		submitted = io_submit();
	}
	while(qcount > 0 && !submitted);

	in_flight += submitted;
	qcount -= submitted;
	assert(!qcount);

	stats.submits += bool(submitted);
	stats.cur_queued -= submitted;
	stats.cur_submits += submitted;
	stats.max_submits = std::max(stats.max_submits, stats.cur_submits);
	assert(stats.cur_queued == qcount);
	assert(stats.cur_submits == in_flight);

	if(idle && submitted > 0 && !handle_set)
		set_handle();

	return submitted;
}
catch(const std::exception &e)
{
	ircd::terminate{ircd::error
	{
		"AIO(%p) system::submit() qcount:%zu :%s",
		this,
		qcount,
		e.what()
	}};

	__builtin_unreachable();
}

size_t
ircd::fs::aio::system::io_submit()
try
{
	#ifdef RB_DEBUG_FS_AIO_SUBMIT_BLOCKING
	const size_t count[3]
	{
		count_queued(op::READ),
		count_queued(op::WRITE),
		count_queued(op::SYNC),
	};

	ctx::syscall_usage_warning warning
	{
		"fs::aio::system::submit(in_flight:%zu qcount:%zu r:%zu w:%zu s:%zu)",
		in_flight,
		qcount,
		count[0],
		count[1],
		count[2],
	};
	#endif

	assert(qcount > 0);
	const auto ret
	{
		syscall<SYS_io_submit>(head.get(), qcount, queue.data())
	};

	#ifdef RB_DEBUG_FS_AIO_SUBMIT_BLOCKING
	stats.stalls += warning.timer.sample() > 0;
	#endif

	assert(!qcount || ret > 0);
	return ret;
}
catch(const std::system_error &e)
{
	log::error
	{
		log, "AIO(%p): io_submit() inflight:%zu qcount:%zu :%s",
		this,
		in_flight,
		qcount,
		e.what()
	};

	switch(e.code().value())
	{
		// Manpages sez that EBADF is thrown if the fd in the FIRST iocb has
		// an issue.
		case int(std::errc::bad_file_descriptor):
			dequeue_one(e.code());
			return 0;

		case int(std::errc::invalid_argument):
		{
			dequeue_all(e.code());
			return 0;
		}
	}

	throw;
}

void
ircd::fs::aio::system::dequeue_all(const std::error_code &ec)
{
	while(qcount > 0)
		dequeue_one(ec);
}

void
ircd::fs::aio::system::dequeue_one(const std::error_code &ec)
{
	assert(qcount > 0);
	iocb *const cb(queue.front());
	std::rotate(begin(queue), begin(queue)+1, end(queue));
	stats.cur_queued--;
	qcount--;

	io_event result {0};
	assert(cb->aio_data == uintptr_t(static_cast<request *>(cb)));
	result.data = cb->aio_data;
	result.obj = uintptr_t(cb);
	result.res = -1;
	result.res2 = ec.value();
	handle_event(result);
}

void
ircd::fs::aio::system::set_handle()
try
{
	assert(!handle_set);
	handle_set = true;
	ecount = 0;

	const asio::mutable_buffers_1 bufs
	{
		&ecount, sizeof(ecount)
	};

	auto handler
	{
		std::bind(&system::handle, this, ph::_1, ph::_2)
	};

	resfd.async_read_some(bufs, ios::handle(handle_descriptor, std::move(handler)));
}
catch(...)
{
	handle_set = false;
	throw;
}

decltype(ircd::fs::aio::system::handle_descriptor)
ircd::fs::aio::system::handle_descriptor
{
	"ircd::fs::aio sigfd",

	// allocator; custom allocation strategy because this handler
	// appears to excessively allocate and deallocate 120 bytes; this
	// is a simple asynchronous operation, we can do better (and perhaps
	// even better than this below).
	[](auto &handler, const size_t &size)
	{
		assert(ircd::fs::aio::system);
		auto &system(*ircd::fs::aio::system);

		if(unlikely(!system.handle_data))
		{
			system.handle_size = size;
			system.handle_data = std::make_unique<uint8_t[]>(size);
		}

		assert(system.handle_size == size);
		return system.handle_data.get();
	},

	// no deallocation; satisfied by class member unique_ptr
	[](auto &handler, void *const &ptr, const auto &size) {}
};

/// Handle notifications that requests are complete.
void
ircd::fs::aio::system::handle(const boost::system::error_code &ec,
                              const size_t bytes)
noexcept try
{
	namespace errc = boost::system::errc;

	assert((bytes == 8 && !ec && ecount >= 1) || (bytes == 0 && ec));
	assert(!ec || ec.category() == asio::error::get_system_category());
	assert(handle_set);
	handle_set = false;

	switch(ec.value())
	{
		case errc::success:
			handle_events();
			break;

		case errc::interrupted:
			break;

		case errc::operation_canceled:
			throw ctx::interrupted();

		default:
			throw_system_error(ec);
	}

	if(in_flight > 0 && !handle_set)
		set_handle();
}
catch(const ctx::interrupted &)
{
	log::debug
	{
		log, "AIO context %p interrupted", this
	};

	ecount = -1;
	dock.notify_all();
}

void
ircd::fs::aio::system::handle_events()
noexcept try
{
	assert(!ctx::current);

	// The number of completed requests available in events[]. This syscall
	// is restarted by us on EINTR. After restart, it may or may not find any ready
	// events but it never blocks to do so.
	const auto count
	{
		syscall_nointr<SYS_io_getevents>(head.get(), 0, event.size(), event.data(), nullptr)
	};

	// The count should be at least 1 event. The only reason to return 0 might
	// be related to an INTR; this assert will find out and may be commented.
	//assert(count > 0);
	assert(count >= 0);

	in_flight -= count;
	stats.cur_submits -= count;
	stats.handles++;
	if(likely(count))
		dock.notify_one();

	for(ssize_t i(0); i < count; ++i)
		handle_event(event[i]);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "AIO(%p) handle_events: %s",
		this,
		e.what()
	};
}

void
ircd::fs::aio::system::handle_event(const io_event &event)
noexcept try
{
	// The kernel always references the iocb in `event.obj`
	auto *const iocb
	{
		reinterpret_cast<struct ::iocb *>(event.obj)
	};

	// We referenced our request (which extends the same iocb anyway)
	// for the kernel to carry through as an opaque in `event.data`.
	auto *const request
	{
		reinterpret_cast<aio::request *>(event.data)
	};

	// Check that everything lines up.
	assert(request && iocb);
	assert(iocb == static_cast<struct ::iocb *>(request));
	assert(request->aio_data);
	assert(request->aio_data == event.data);
	assert(request->aio_data == iocb->aio_data);
	assert(request->aio_data == uintptr_t(request));

	// Assert that we understand the return-value semantics of this interface.
	assert(event.res2 >= 0);
	assert(event.res == -1 || event.res2 == 0);

	// Set result indicators
	request->retval = std::max(event.res, -1LL);
	request->errcode = event.res >= -1? event.res2 : std::abs(event.res);

	// Notify the waiting context. Note that we are on the main async stack
	// but it is safe to notify from here.
	assert(request->waiter);
	ctx::notify(*request->waiter);
	stats.events++;
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Unhandled request(%lu) event(%p) error: %s",
		event.data,
		&event,
		e.what()
	};
}

size_t
ircd::fs::aio::system::request_avail()
const
{
	assert(request_count() <= max_events());
	return max_events() - request_count();
}

size_t
ircd::fs::aio::system::request_count()
const
{
	return qcount + in_flight;
}

size_t
ircd::fs::aio::system::max_submit()
const
{
	return queue.size();
}

size_t
ircd::fs::aio::system::max_events()
const
{
	return event.size();
}
