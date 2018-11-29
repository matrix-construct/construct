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
#include "aio.h"

namespace ircd::fs::aio
{
	static int reqprio(int);
}

///////////////////////////////////////////////////////////////////////////////
//
// ircd/aio.h (internal)
//

//
// request::fsync
//

ircd::fs::aio::request::fsync::fsync(const int &fd,
                                     const fsync_opts &opts)
:request{fd}
{
	aio_reqprio = reqprio(opts.priority);
	aio_lio_opcode = IOCB_CMD_FSYNC;

	aio_buf = 0;
	aio_nbytes = 0;
	aio_offset = 0;
}

void
ircd::fs::aio::fsync(const fd &fd,
                     const fsync_opts &opts)
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
                                       const fsync_opts &opts)
:request{fd}
{
	aio_reqprio = reqprio(opts.priority);
	aio_lio_opcode = IOCB_CMD_FDSYNC;

	aio_buf = 0;
	aio_nbytes = 0;
	aio_offset = 0;
}

void
ircd::fs::aio::fdsync(const fd &fd,
                      const fsync_opts &opts)
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
                                   const vector_view<const struct ::iovec> &iov,
                                   const read_opts &opts)
:request{fd}
{
	aio_reqprio = reqprio(opts.priority);
	aio_lio_opcode = IOCB_CMD_PREADV;

	aio_buf = uintptr_t(iov.data());
	aio_nbytes = iov.size();
	aio_offset = opts.offset;
}

size_t
ircd::fs::aio::read(const fd &fd,
                    const mutable_buffers &bufs,
                    const read_opts &opts)
{
	aio::request::read request
	{
		fd, make_iov(bufs), opts
	};

	const size_t bytes
	{
		request()
	};

	aio::stats.read_bytes += bytes;
	aio::stats.reads++;
	return bytes;
}

//
// request::write
//

ircd::fs::aio::request::write::write(const int &fd,
                                     const vector_view<const struct ::iovec> &iov,
                                     const write_opts &opts)
:request{fd}
{
	aio_reqprio = reqprio(opts.priority);
	aio_lio_opcode = IOCB_CMD_PWRITEV;

	aio_buf = uintptr_t(iov.data());
	aio_nbytes = iov.size();
	aio_offset = opts.offset;
}

size_t
ircd::fs::aio::write(const fd &fd,
                     const const_buffers &bufs,
                     const write_opts &opts)
{
	aio::request::write request
	{
		fd, make_iov(bufs), opts
	};

	#ifndef _NDEBUG
	const size_t req_bytes
	{
		fs::bytes(request.iovec())
	};
	#endif

	const size_t bytes
	{
		request()
	};

	// Does linux ever not complete all bytes for an AIO?
	assert(bytes == req_bytes);

	aio::stats.write_bytes += bytes;
	aio::stats.writes++;
	return bytes;
}

//
// request::prefetch
//

void
ircd::fs::aio::prefetch(const fd &fd,
                        const size_t &size,
                        const read_opts &opts)
{

}

//
// internal util
//

int
ircd::fs::aio::reqprio(int input)
{
	// no use for negative values yet; make them zero.
	input = std::max(input, 0);

	// value is reduced to system maximum.
	input = std::min(input, ircd::info::aio_reqprio_max);

	return input;
}

//
// kernel
//

decltype(ircd::fs::aio::kernel::MAX_EVENTS)
ircd::fs::aio::kernel::MAX_EVENTS
{
	512
};

//
// kernel::kernel
//

ircd::fs::aio::kernel::kernel()
try
:resfd
{
	ios::get(), int(syscall(::eventfd, semval, EFD_NONBLOCK))
}
{
	syscall<SYS_io_setup>(MAX_EVENTS, &idp);
	set_handle();

	log::debug
	{
		"Established AIO context %p", this
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

ircd::fs::aio::kernel::~kernel()
noexcept try
{
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
ircd::fs::aio::kernel::interrupt()
{
	if(!resfd.is_open())
		return false;

	resfd.cancel();
	return true;
}

bool
ircd::fs::aio::kernel::wait()
{
	if(!resfd.is_open())
		return false;

	log::debug
	{
		"Waiting for AIO context %p", this
	};

	dock.wait([this]
	{
		return semval == uint64_t(-1);
	});

	return true;
}

void
ircd::fs::aio::kernel::set_handle()
{
	semval = 0;

	const asio::mutable_buffers_1 bufs
	{
		&semval, sizeof(semval)
	};

	auto handler
	{
		std::bind(&kernel::handle, this, ph::_1, ph::_2)
	};

	asio::async_read(resfd, bufs, std::move(handler));
}

/// Handle notifications that requests are complete.
void
ircd::fs::aio::kernel::handle(const boost::system::error_code &ec,
                              const size_t bytes)
noexcept try
{
	namespace errc = boost::system::errc;

	assert((bytes == 8 && !ec && semval >= 1) || (bytes == 0 && ec));
	assert(!ec || ec.category() == asio::error::get_system_category());

	switch(ec.value())
	{
		case errc::success:
			handle_events();
			break;

		case errc::operation_canceled:
			throw ctx::interrupted();

		default:
			throw_system_error(ec);
	}

	set_handle();
}
catch(const ctx::interrupted &)
{
	log::debug
	{
		"AIO context %p interrupted", this
	};

	semval = -1;
	dock.notify_all();
}

void
ircd::fs::aio::kernel::handle_events()
noexcept try
{
	assert(!ctx::current);
	thread_local std::array<io_event, MAX_EVENTS> event;

	// The number of completed requests available in events[]. This syscall
	// is restarted on EINTR. After restart, it may or may not find any ready
	// events but it never blocks to do so.
	const auto count
	{
		syscall_nointr<SYS_io_getevents>(idp, 0, event.size(), event.data(), nullptr)
	};

	// The count should be at least 1 event. The only reason to return 0 might
	// be related to an INTR; this assert will find out and may be commented.
	//assert(count > 0);
	assert(count >= 0);

	aio::stats.handles++;
	aio::stats.events += count;

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
ircd::fs::aio::kernel::handle_event(const io_event &event)
noexcept try
{
	// Our extended control block is passed in event.data
	auto &request
	{
		*reinterpret_cast<aio::request *>(event.data)
	};

	assert(reinterpret_cast<iocb *>(event.obj) == static_cast<iocb *>(&request));
	assert(event.res2 >= 0);
	assert(event.res == -1 || event.res2 == 0);

	// Set result indicators
	request.retval = std::max(event.res, -1LL);
	request.errcode = event.res >= -1? event.res2 : std::abs(event.res);

	// Notify the waiting context. Note that we are on the main async stack
	// but it is safe to notify from here. The waiter may be null if it left.
	assert(!request.waiter || request.waiter != ctx::current);
	assert(ctx::current == nullptr);
	if(likely(request.waiter))
		ctx::notify(*request.waiter);

/*
	log::debug
	{
		"AIO request(%p) fd:%d op:%d bytes:%lu off:%ld prio:%d ctx:%p result: bytes:%ld errno:%ld",
		request,
		request->aio_fildes,
		request->aio_lio_opcode,
		request->aio_nbytes,
		request->aio_offset,
		request->aio_reqprio,
		request->waiter,
		request->retval,
		request->errcode
	};
*/
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

//
// request
//

ircd::fs::aio::request::request(const int &fd)
:iocb{0}
{
	assert(context);
	assert(ctx::current);

	aio_flags = IOCB_FLAG_RESFD;
	aio_resfd = context->resfd.native_handle();
	aio_fildes = fd;
	aio_data = uintptr_t(this);
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
	io_event result {0};
	const auto &cb{static_cast<iocb *>(this)};

	assert(context);
	syscall_nointr<SYS_io_cancel>(context->idp, cb, &result);

	aio::stats.cancel_bytes += bytes(iovec());
	aio::stats.cancel++;

	context->handle_event(result);
}

/// Submit a request and properly yield the ircd::ctx. When this returns the
/// result will be available or an exception will be thrown.
size_t
ircd::fs::aio::request::operator()()
try
{
	assert(context);
	assert(ctx::current);
	assert(waiter == ctx::current);

	struct iocb *const cbs[]
	{
		static_cast<iocb *>(this)
	};

	const size_t submitted_bytes
	{
		bytes(iovec())
	};

	syscall<SYS_io_submit>(context->idp, 1, &cbs);

	// Update stats for submission phase
	aio::stats.requests_bytes += submitted_bytes;
	aio::stats.requests++;

	// Block for completion
	while(retval == std::numeric_limits<ssize_t>::min())
		ctx::wait();

	// Update stats for completion phase.
	aio::stats.complete_bytes += submitted_bytes;
	aio::stats.complete++;

	if(retval == -1)
	{
		aio::stats.errors++;
		aio::stats.errors_bytes += submitted_bytes;

		throw fs::error
		{
			make_error_code(errcode)
		};
	}

	return size_t(retval);
}
catch(const ctx::interrupted &e)
{
	// When the ctx is interrupted we're obligated to cancel the request.
	// The handler callstack is invoked directly from here by cancel() for
	// what it's worth but we rethrow the interrupt anyway.
	cancel();
	throw;
}
catch(const ctx::terminated &)
{
	cancel();
	throw;
}

ircd::vector_view<const struct ::iovec>
ircd::fs::aio::request::iovec()
const
{
	return
	{
		reinterpret_cast<const ::iovec *>(aio_buf), aio_nbytes
	};
}
