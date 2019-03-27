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
	info::kversion[0] >= 4 &&
	info::kversion[1] >= 18
};

/// True if IOCB_CMD_FDSYNC is supported by AIO. If this is false then
/// fs::fsync_opts::async=true flag is ignored.
decltype(ircd::fs::aio::support_fdsync)
ircd::fs::aio::support_fdsync
{
	info::kversion[0] >= 4 &&
	info::kversion[1] >= 18
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
// fs_aio.h
//

//
// request::fsync
//

ircd::fs::aio::request::fsync::fsync(const int &fd,
                                     const sync_opts &opts)
:request{fd, &opts}
{
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

	// Make request; blocks ircd::ctx until completed or throw.
	const size_t bytes
	{
		request()
	};

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
	aio_lio_opcode = IOCB_CMD_PWRITEV;

	aio_buf = uintptr_t(iov.data());
	aio_nbytes = iov.size();
	aio_offset = opts.offset;

	#if defined(HAVE_PWRITEV2) && defined(RWF_APPEND)
	if(support_append && opts.offset == -1)
	{
		// AIO departs from pwritev2() behavior and EINVAL's on -1.
		aio_offset = 0;
		aio_rw_flags |= RWF_APPEND;
	}
	#endif

	#if defined(HAVE_PWRITEV2) && defined(RWF_DSYNC)
	if(support_dsync && opts.sync && !opts.metadata)
		aio_rw_flags |= RWF_DSYNC;
	#endif

	#if defined(HAVE_PWRITEV2) && defined(RWF_SYNC)
	if(support_sync && opts.sync && opts.metadata)
		aio_rw_flags |= RWF_SYNC;
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

	#ifndef _NDEBUG
	const size_t req_bytes
	{
		fs::bytes(request.iovec())
	};
	#endif

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
	assert(bytes == req_bytes);

	stats.bytes_write += bytes;
	stats.writes++;
	return bytes;
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

	#if defined(HAVE_PWRITEV2) && defined(HAVE_PREADV2) && defined(RWF_HIPRI)
	if(support_hipri && aio_reqprio == reqprio(opts::highest_priority))
		aio_rw_flags |= RWF_HIPRI;
	#endif

	#if defined(HAVE_PWRITEV2) && defined(HAVE_PREADV2) && defined(RWF_NOWAIT)
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
void
ircd::fs::aio::request::cancel()
{
	assert(system);
	system->cancel(*this);
	stats.bytes_cancel += bytes(iovec());
	stats.cancel++;
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

	const auto &curcnt(stats.requests - stats.complete);
	stats.max_requests = std::max(stats.max_requests, curcnt);

	// Wait here until there's room to submit a request
	system->dock.wait([]
	{
		return system->request_avail() > 0;
	});

	// Submit to system
	system->submit(*this);

	// Wait for completion
	system->wait(*this);
	assert(retval <= ssize_t(submitted_bytes));

	// Update stats for completion phase.
	stats.bytes_complete += submitted_bytes;
	stats.complete++;

	if(likely(retval != -1))
		return size_t(retval);

	stats.errors++;
	stats.bytes_errors += submitted_bytes;
	thread_local char errbuf[2][512]; fmt::sprintf
	{
		errbuf[0], "fd:%d size:%zu off:%zd op:%u pri:%u #%lu",
		aio_fildes,
		aio_nbytes,
		aio_offset,
		aio_lio_opcode,
		aio_reqprio,
		errcode
	};

	throw std::system_error
	{
		make_error_code(errcode), errbuf[0]
	};
}

bool
ircd::fs::aio::request::completed()
const
{
	return retval != std::numeric_limits<decltype(retval)>::min();
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
	max_submit
}
,resfd
{
	ios::get(), int(syscall(::eventfd, ecount, eventfd_flags))
}
{
	syscall<SYS_io_setup>(this->max_events(), &idp);

	const aio_ring *const ring
	{
		reinterpret_cast<const aio_ring *>(idp)
	};

	assert(ring->magic == aio_ring::MAGIC);

	log::debug
	{
		"Established AIO(%p) context (fd:%d max_events:%zu max_submit:%zu)",
		this,
		int(resfd.native_handle()),
		this->max_events(),
		this->max_submit(),
	};
}
catch(const std::exception &e)
{
	log::error
	{
		"Error starting AIO context %p :%s",
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

	syscall<SYS_io_destroy>(idp);
}
catch(const std::exception &e)
{
	log::critical
	{
		"Error shutting down AIO context %p :%s",
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
		"Waiting for AIO context %p", this
	};

	dock.wait([this]
	{
		return ecount == uint64_t(-1);
	});

	assert(request_count() == 0);
	return true;
}

void
ircd::fs::aio::system::wait(request &request)
try
{
	assert(ctx::current == request.waiter);
	while(request.retval == std::numeric_limits<ssize_t>::min())
		ctx::wait();
}
catch(const ctx::interrupted &e)
{
	// When the ctx is interrupted we're obligated to cancel the request.
	// The handler callstack is invoked directly from here by cancel() for
	// what it's worth but we rethrow the interrupt anyway.
	if(!request.completed())
		request.cancel();

	throw;
}
catch(const ctx::terminated &)
{
	if(!request.completed())
		request.cancel();

	throw;
}

void
ircd::fs::aio::system::cancel(request &request)
{
	assert(request.retval == std::numeric_limits<ssize_t>::min());
	assert(request.aio_data == uintptr_t(&request));

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
		syscall_nointr<SYS_io_cancel>(idp, cb, &result);
		in_flight--;
		stats.cur_submits--;
		dock.notify_one();
	}

	handle_event(result);
}

void
ircd::fs::aio::system::submit(request &request)
{
	assert(request.opts);
	assert(qcount < queue.size());
	assert(qcount + in_flight < max_events());
	assert(request.aio_data == uintptr_t(&request));

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
		ircd::post(descriptor, std::move(handler));
	}
}

/// The chaser is posted to the IRCd event loop after the first request is
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

/// The submitter submits all queued requests and resets the count.
size_t
ircd::fs::aio::system::submit()
try
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
catch(const std::system_error &e)
{
	switch(e.code().value())
	{
		// EAGAIN may be thrown to prevent blocking. TODO: handle
		case int(std::errc::resource_unavailable_try_again):
			break;
	}

	ircd::terminate{ircd::error
	{
		"AIO(%p) system::submit() qcount:%zu :%s",
		this,
		qcount,
		e.what()
	}};
}

size_t
ircd::fs::aio::system::io_submit()
try
{
	const ctx::slice_usage_warning message
	{
		"fs::aio::system::submit(in_flight:%zu qcount:%zu)",
		in_flight,
		qcount
	};

	return syscall<SYS_io_submit>(idp, qcount, queue.data());
}
catch(const std::system_error &e)
{
	switch(e.code().value())
	{
		// Manpages sez that EBADF is thrown if the fd in the FIRST iocb has
		// an issue. TODO: handle this by tossing the first iocb and continue.
		case int(std::errc::bad_file_descriptor):
			dequeue_one(e.code());
			return 0;

		// Do we have to kill all the requests just because one has EINVAL? Can
		// we just find the one at issue?
		case int(std::errc::invalid_argument):
			dequeue_all(e.code());
			return 0;
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

	static ios::descriptor descriptor
	{
		"ircd::fs::aio sigfd"
	};

	resfd.async_read_some(bufs, ios::handle(descriptor, std::move(handler)));
}
catch(...)
{
	handle_set = false;
	throw;
}

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
		"AIO context %p interrupted", this
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
		syscall_nointr<SYS_io_getevents>(idp, 0, event.size(), event.data(), nullptr)
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
		"AIO(%p) handle_events: %s",
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
		"Unhandled request(%lu) event(%p) error: %s",
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
