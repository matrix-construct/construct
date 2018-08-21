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
#include "aio.h"

//
// request::fsync
//

ircd::fs::aio::request::fsync::fsync(const int &fd,
                                     const fsync_opts &opts)
:request{fd}
{
	aio_reqprio = opts.priority;
	aio_lio_opcode = IOCB_CMD_FSYNC;

	aio_buf = 0;
	aio_nbytes = 0;
	aio_offset = 0;
}

void
ircd::fs::fsync__aio(const fd &fd,
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
	aio_reqprio = opts.priority;
	aio_lio_opcode = IOCB_CMD_FDSYNC;

	aio_buf = 0;
	aio_nbytes = 0;
	aio_offset = 0;
}

void
ircd::fs::fdsync__aio(const fd &fd,
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
                                   const mutable_buffer &buf,
                                   const read_opts &opts)
:request{fd}
{
	aio_reqprio = opts.priority;
	aio_lio_opcode = IOCB_CMD_PREAD;

	aio_buf = uintptr_t(buffer::data(buf));
	aio_nbytes = buffer::size(buf);
	aio_offset = opts.offset;
}

ircd::const_buffer
ircd::fs::read__aio(const fd &fd,
                    const mutable_buffer &buf,
                    const read_opts &opts)
{
	aio::request::read request
	{
		fd, buf, opts
	};

	const size_t bytes
	{
		request()
	};

	const const_buffer view
	{
		const_cast<const char *>(data(buf)), bytes
	};

	return view;
}

//
// request::write
//

ircd::fs::aio::request::write::write(const int &fd,
                                     const const_buffer &buf,
                                     const write_opts &opts)
:request{fd}
{
	aio_reqprio = opts.priority;
	aio_lio_opcode = IOCB_CMD_PWRITE;

	aio_buf = uintptr_t(buffer::data(buf));
	aio_nbytes = buffer::size(buf);
	aio_offset = opts.offset;
}

ircd::const_buffer
ircd::fs::write__aio(const fd &fd,
                     const const_buffer &buf,
                     const write_opts &opts)
{
	aio::request::write request
	{
		fd, buf, opts
	};

	const size_t bytes
	{
		request()
	};

	const const_buffer view
	{
		data(buf), bytes
	};

	return view;
}

//
// request::prefetch
//

void
ircd::fs::prefetch__aio(const fd &fd,
                        const size_t &size,
                        const read_opts &opts)
{

}

//
// aio
//

decltype(ircd::fs::aio::MAX_EVENTS)
ircd::fs::aio::MAX_EVENTS
{
	512
};

//
// aio::aio
//

ircd::fs::aio::aio()
:resfd
{
	*ircd::ios, int(syscall(::eventfd, semval, EFD_NONBLOCK))
}
{
	syscall<SYS_io_setup>(MAX_EVENTS, &idp);
	set_handle();
}

ircd::fs::aio::~aio()
noexcept
{
	interrupt();
	wait();

	boost::system::error_code ec;
	resfd.close(ec);

	syscall<SYS_io_destroy>(idp);
}

bool
ircd::fs::aio::interrupt()
{
	if(!resfd.is_open())
		return false;

	resfd.cancel();
	return true;
}

bool
ircd::fs::aio::wait()
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
ircd::fs::aio::set_handle()
{
	semval = 0;
	const asio::mutable_buffers_1 bufs(&semval, sizeof(semval));
	auto handler{std::bind(&aio::handle, this, ph::_1, ph::_2)};
	asio::async_read(resfd, bufs, std::move(handler));
}

/// Handle notifications that requests are complete.
void
ircd::fs::aio::handle(const boost::system::error_code &ec,
                      const size_t bytes)
noexcept try
{
	assert((bytes == 8 && !ec && semval >= 1) || (bytes == 0 && ec));
	assert(!ec || ec.category() == asio::error::get_system_category());

	switch(ec.value())
	{
		case boost::system::errc::success:
			handle_events();
			break;

		case boost::system::errc::operation_canceled:
			throw ctx::interrupted();

		default:
			throw boost::system::system_error(ec);
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
ircd::fs::aio::handle_events()
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
ircd::fs::aio::handle_event(const io_event &event)
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
	assert(aioctx);
	assert(ctx::current);

	aio_flags = IOCB_FLAG_RESFD;
	aio_resfd = aioctx->resfd.native_handle();
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

	assert(aioctx);
	syscall_nointr<SYS_io_cancel>(aioctx->idp, cb, &result);
	aioctx->handle_event(result);
}

/// Submit a request and properly yield the ircd::ctx. When this returns the
/// result will be available or an exception will be thrown.
size_t
ircd::fs::aio::request::operator()()
try
{
	assert(aioctx);
	assert(ctx::current);
	assert(waiter == ctx::current);

	struct iocb *const cbs[]
	{
		static_cast<iocb *>(this)
	};

	syscall<SYS_io_submit>(aioctx->idp, 1, &cbs); do
	{
		ctx::wait();
	}
	while(retval == std::numeric_limits<ssize_t>::min());

	if(retval == -1)
		throw_system_error(errcode);

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
