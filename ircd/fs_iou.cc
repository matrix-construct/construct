// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <sys/syscall.h>
#include <sys/eventfd.h>
#include "fs_iou.h"

decltype(ircd::fs::iou::MAX_EVENTS)
ircd::fs::iou::MAX_EVENTS
{
	128L //TODO: get this info
};

decltype(ircd::fs::iou::max_events)
ircd::fs::iou::max_events
{
	{ "name",     "ircd.fs.iou.max_events"  },
	{ "default",  long(iou::MAX_EVENTS)     },
	{ "persist",  false                     },
};

decltype(ircd::fs::iou::max_submit)
ircd::fs::iou::max_submit
{
	{ "name",     "ircd.fs.iou.max_submit"  },
	{ "default",  0L                        },
	{ "persist",  false                     },
};

//
// init
//

ircd::fs::iou::init::init()
{
	assert(!system);
	if(!iou::enable)
		return;

	system = new struct iou::system
	(
		size_t(max_events),
		size_t(max_submit)
	);
}

ircd::fs::iou::init::~init()
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
// unit is conditionally compiled and linked on IOU-supporting platforms.

ircd::fs::op
ircd::fs::iou::translate(const int &val)
{
	switch(val)
	{
		case IORING_OP_NOP:               return op::NOOP;
		case IORING_OP_READV:             return op::READ;
		case IORING_OP_WRITEV:            return op::WRITE;
		case IORING_OP_FSYNC:             return op::SYNC;
		case IORING_OP_READ_FIXED:        return op::READ;
		case IORING_OP_WRITE_FIXED:       return op::WRITE;
		case IORING_OP_POLL_ADD:          return op::NOOP;
		case IORING_OP_POLL_REMOVE:       return op::NOOP;
		case IORING_OP_SYNC_FILE_RANGE:   return op::SYNC;
	}

	return op::NOOP;
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/iou.h
//

size_t
ircd::fs::iou::count(const op &op)
{
	return 0;
}

size_t
ircd::fs::iou::count(const state &state)
{
	return 0;
}

size_t
ircd::fs::iou::count(const state &state,
                     const op &op)
{
	return 0;
}

bool
ircd::fs::iou::for_each(const state &state,
                        const std::function<bool (const request &)> &closure)
{
	assert(system);
	return true;
}

bool
ircd::fs::iou::for_each(const std::function<bool (const request &)> &closure)
{
	assert(system);
	return true;
}

struct ::io_uring_sqe &
ircd::fs::iou::sqe(request &request)
{
	assert(system);
	::io_uring_sqe *const ret
	{
		nullptr
	};

	if(request.id < 0)
		throw std::out_of_range
		{
			"request has no entry on the submit queue."
		};

	assert(ret);
	return *ret;
}

const struct ::io_uring_sqe &
ircd::fs::iou::sqe(const request &request)
{
	assert(system);
	const ::io_uring_sqe *const ret
	{
		nullptr
	};

	if(request.id < 0)
		throw std::out_of_range
		{
			"request has no entry on the submit queue."
		};

	assert(ret);
	return *ret;
}

ircd::string_view
ircd::fs::iou::reflect(const state &s)
{
	switch(s)
	{
		case state::INVALID:     return "INVALID";
		case state::QUEUED:      return "QUEUED";
		case state::SUBMITTED:   return "SUBMITTED";
		case state::COMPLETED:   return "COMPLETED";
		case state::_NUM:        break;
	}

	return "?????";
}

ircd::fs::const_iovec_view
ircd::fs::iou::iovec(const request &request)
{
	return
	{
		//reinterpret_cast<const ::iovec *>(iou_buf), iou_nbytes
	};
}

//
// request::request
//

ircd::fs::iou::request::request(const fs::fd &fd,
                                const const_iovec_view &iov,
                                const fs::opts *const &opts)
:opts
{
	opts
}
,op
{
	fs::op::NOOP
}
{
}

ircd::fs::iou::request::~request()
noexcept
{
}

//
// system::system
//

ircd::fs::iou::system::system(const size_t &max_events,
                              const size_t &max_submit)
try
:p
{
	0
}
,fd
{
	int(syscall<__NR_io_uring_setup>(max_events, &p))
}
,sq_len
{
	p.sq_off.array + p.sq_entries * sizeof(uint32_t)
}
,cq_len
{
	p.cq_off.cqes + p.cq_entries * sizeof(::io_uring_cqe)
}
,sqe_len
{
	p.sq_entries * sizeof(::io_uring_sqe)
}
,sq_p
{
	[this]
	{
		static const auto prot(PROT_READ | PROT_WRITE);
		static const auto flags(MAP_SHARED | MAP_POPULATE);
		void *const &map
		{
			::mmap(NULL, sq_len, prot, flags, fd, IORING_OFF_SQ_RING)
		};

		if(unlikely(map == MAP_FAILED))
		{
			throw_system_error(errno);
			__builtin_unreachable();
		}

		return reinterpret_cast<uint8_t *>(map);
	}(),
	[this](uint8_t *const ptr)
	{
		syscall(::munmap, ptr, sq_len);
	}
}
,cq_p
{
	[this]
	{
		static const auto prot(PROT_READ | PROT_WRITE);
		static const auto flags(MAP_SHARED | MAP_POPULATE);
		void *const &map
		{
			::mmap(NULL, cq_len, prot, flags, fd, IORING_OFF_CQ_RING)
		};

		if(unlikely(map == MAP_FAILED))
		{
			throw_system_error(errno);
			__builtin_unreachable();
		}

		return reinterpret_cast<uint8_t *>(map);
	}(),
	[this](uint8_t *const ptr)
	{
		syscall(::munmap, ptr, cq_len);
	}
}
,sqe_p
{
	[this]
	{
		static const auto prot(PROT_READ | PROT_WRITE);
		static const auto flags(MAP_SHARED | MAP_POPULATE);
		void *const &map
		{
			::mmap(NULL, sqe_len, prot, flags, fd, IORING_OFF_SQES)
		};

		if(unlikely(map == MAP_FAILED))
		{
			throw_system_error(errno);
			__builtin_unreachable();
		}

		return reinterpret_cast<uint8_t *>(map);
	}(),
	[this](uint8_t *const ptr)
	{
		syscall(::munmap, ptr, sqe_len);
	}
}
,head
{
	reinterpret_cast<uint32_t *>(sq_p.get() + p.sq_off.head),
	reinterpret_cast<uint32_t *>(cq_p.get() + p.cq_off.head),
}
,tail
{
	reinterpret_cast<uint32_t *>(sq_p.get() + p.sq_off.tail),
	reinterpret_cast<uint32_t *>(cq_p.get() + p.cq_off.tail),
}
,ring_mask
{
	reinterpret_cast<uint32_t *>(sq_p.get() + p.sq_off.ring_mask),
	reinterpret_cast<uint32_t *>(cq_p.get() + p.cq_off.ring_mask),
}
,ring_entries
{
	reinterpret_cast<uint32_t *>(sq_p.get() + p.sq_off.ring_entries),
	reinterpret_cast<uint32_t *>(cq_p.get() + p.cq_off.ring_entries),
}
,flags
{
	reinterpret_cast<uint32_t *>(sq_p.get() + p.sq_off.flags),
	nullptr
}
,dropped
{
	reinterpret_cast<uint32_t *>(sq_p.get() + p.sq_off.dropped),
	nullptr
}
,overflow
{
	nullptr,
	reinterpret_cast<uint32_t *>(cq_p.get() + p.cq_off.overflow),
}
,sq
{
	reinterpret_cast<uint32_t *>(sq_p.get() + p.sq_off.array)
}
,sqe
{
	reinterpret_cast<::io_uring_sqe *>(sqe_p.get())
}
,cqe
{
	reinterpret_cast<::io_uring_cqe *>(cq_p.get() + p.cq_off.cqes)
}
,ev_count
{
	0
}
,ev_fd
{
	ios::get(), int(syscall(::eventfd, ev_count, EFD_CLOEXEC | EFD_NONBLOCK))
}
,handle_set
{
	false
}
,handle_size
{
	0
}
{
	log::debug
	{
		log, "io_uring sq_entries:%u cq_entries:%u flags:%u sq_thread_cpu:%u sq_thread_idle:%u",
		p.sq_entries,
		p.cq_entries,
		p.flags,
		p.sq_thread_cpu,
		p.sq_thread_idle,
	};

	log::debug
	{
		log, "io_uring maps sq:%p len:%zu sqe:%p len:%zu cq:%p len:%zu",
		sq_p.get(),
		sq_len,
		sqe_p.get(),
		sqe_len,
		cq_p.get(),
		cq_len,
	};

	log::debug
	{
		log, "io_sqring head:%u tail:%u ring_mask:%u ring_entries:%u flags:%u dropped:%u array:%u map:%p len:%zu",
		p.sq_off.head,
		p.sq_off.tail,
		p.sq_off.ring_mask,
		p.sq_off.ring_entries,
		p.sq_off.flags,
		p.sq_off.dropped,
		p.sq_off.array,
		sq_p.get(),
		sq_len,
	};

	log::debug
	{
		log, "io_cqring head:%u tail:%u ring_mask:%u ring_entries:%u overflow:%u cqes:%u map:%p len:%zu",
		p.cq_off.head,
		p.cq_off.tail,
		p.cq_off.ring_mask,
		p.cq_off.ring_entries,
		p.cq_off.overflow,
		p.cq_off.cqes,
		cq_p.get(),
		cq_len,
	};

	assert(0);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Error starting iou context %p :%s",
		(const void *)this,
		e.what()
	};
}

ircd::fs::iou::system::~system()
noexcept try
{
	const ctx::uninterruptible::nothrow ui;

	interrupt();
	wait();

	boost::system::error_code ec;
	ev_fd.close(ec);
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Error shutting down iou context %p :%s",
		(const void *)this,
		e.what()
	};
}

bool
ircd::fs::iou::system::interrupt()
{
	if(!ev_fd.is_open())
		return false;

	if(handle_set)
		ev_fd.cancel();
	else
		ev_count = -1;

	return true;
}

bool
ircd::fs::iou::system::wait()
{
	if(!ev_fd.is_open())
		return false;

	log::debug
	{
		log, "Waiting for iou context %p", this
	};

	dock.wait([this]
	{
		return ev_count == uint64_t(-1);
	});

	return true;
}

void
ircd::fs::iou::system::set_handle()
try
{
	assert(!handle_set);
	handle_set = true;
	ev_count = 0;

	const asio::mutable_buffers_1 bufs
	{
		&ev_count, sizeof(ev_count)
	};

	auto handler
	{
		std::bind(&system::handle, this, ph::_1, ph::_2)
	};

	ev_fd.async_read_some(bufs, ios::handle(handle_descriptor, std::move(handler)));
}
catch(...)
{
	handle_set = false;
	throw;
}

decltype(ircd::fs::iou::system::handle_descriptor)
ircd::fs::iou::system::handle_descriptor
{
	"ircd.fs.iou.sigfd",

	// allocator; custom allocation strategy because this handler
	// appears to excessively allocate and deallocate 120 bytes; this
	// is a simple asynchronous operation, we can do better (and perhaps
	// even better than this below).
	[](ios::handler &handler, const size_t &size) -> void *
	{
		assert(ircd::fs::iou::system);
		auto &system(*ircd::fs::iou::system);

		if(unlikely(!system.handle_data))
		{
			system.handle_size = size;
			system.handle_data = std::make_unique<uint8_t[]>(size);
		}

		assert(system.handle_size == size);
		return system.handle_data.get();
	},

	// no deallocation; satisfied by class member unique_ptr
	[](ios::handler &handler, void *const &ptr, const size_t &size) -> void {}
};

/// Handle notifications that requests are complete.
void
ircd::fs::iou::system::handle(const boost::system::error_code &ec,
                              const size_t bytes)
noexcept try
{
	namespace errc = boost::system::errc;

	assert((bytes == 8 && !ec && ev_count >= 1) || (bytes == 0 && ec));
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
			__builtin_unreachable();

		default:
			throw_system_error(ec);
			__builtin_unreachable();
	}

	set_handle();
}
catch(const ctx::interrupted &)
{
	log::debug
	{
		log, "iou context %p interrupted", this
	};

	ev_count = -1;
	dock.notify_all();
}

void
ircd::fs::iou::system::handle_events()
noexcept try
{
	assert(!ctx::current);


}
catch(const std::exception &e)
{
	log::error
	{
		log, "iou(%p) handle_events: %s",
		this,
		e.what()
	};
}
