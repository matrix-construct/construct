// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#pragma once
#define HAVE_AIO_H

#include <RB_INC_SYS_SYSCALL_H
#include <RB_INC_SYS_EVENTFD_H
#include <linux/aio_abi.h>

/// AIO context instance. Right now this is a singleton with an extern
/// instance at fs::aioctx.
struct ircd::fs::aio
{
	struct request;

	/// Maximum number of events we can submit to kernel
	static constexpr const size_t &MAX_EVENTS
	{
		64
	};

	/// The semaphore value for the eventfd which we keep here.
	uint64_t semval{0};

	/// An eventfd which will be notified by the kernel; we integrate this with
	/// the ircd io_service core epoll() event loop. The EFD_SEMAPHORE flag is
	/// not used to reduce the number of triggers. We can collect multiple AIO
	/// completions after a single trigger to this fd. With EFD_SEMAPHORE, we
	/// would collect all the completions on the first trigger and then
	/// continue to get polled. Because EFD_SEMAPHORE is not set, the semval
	/// which is kept above will reflect a hint for how many AIO's are done.
	asio::posix::stream_descriptor resfd
	{
		*ircd::ios, int(syscall(::eventfd, semval, EFD_NONBLOCK))
	};

	/// Handler to the io context we submit requests to the kernel with
	aio_context_t idp
	{
		0
	};

	// Callback stack invoked when the sigfd is notified of completed events.
	void handle_event(const io_event &) noexcept;
	void handle_events() noexcept;
	void handle(const error_code &, const size_t) noexcept;

	void set_handle();

	aio()
	{
		syscall<SYS_io_setup>(MAX_EVENTS, &idp);
		set_handle();
	}

	~aio() noexcept
	{
		resfd.cancel();
		syscall<SYS_io_destroy>(idp);
	}
};

/// Generic request control block.
struct ircd::fs::aio::request
:iocb
{
	struct read;
	struct write;

	ssize_t retval {0};
	ssize_t errcode {0};
	ctx::ctx *waiter {nullptr};
	bool close_fd {false};
	bool free_req {false};

	/// called if close_fd is true
	void close_fildes() noexcept;

	/// Overriden by types of requests
	virtual void handle() = 0;

  public:
	// Submit a request. ctx overload will properly yield
	size_t operator()(ctx::ctx &waiter);
	void operator()();

	void cancel();

	request(const int &fd);
	virtual ~request() noexcept;
};

ircd::fs::aio::request::request(const int &fd)
:iocb{0}
{
	assert(aioctx);
	aio_flags = IOCB_FLAG_RESFD;
	aio_resfd = aioctx->resfd.native_handle();
	aio_fildes = fd;
}

/// vtable base
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

/// Submit a request and properly yield the ircd::ctx. The ctx argument
/// must be the currently running ctx (or ctx::current) for now; there is
/// no other usage. When this returns the result will be available or
/// exception will be thrown.
size_t
ircd::fs::aio::request::operator()(ctx::ctx &waiter)
try
{
	// Cooperation strategy is to set this->waiter to the ctx and wait for
	// notification with this->waiter set to null to ignore spurious notes.
	assert(&waiter == ctx::current);
	this->waiter = &waiter;
	operator()(); do
	{
		ctx::wait();
	}
	while(this->waiter);

	if(retval == -1)
		throw_system_error(errcode);

	return size_t(retval);
}
catch(const ctx::interrupted &e)
{
	// When the ctx is interrupted we're obligated to cancel the request.
	// The handler callstack is invoked directly from here by cancel() for
	// what it's worth but we rethrow the interrupt anyway.
	this->waiter = nullptr;
	cancel();
	throw;
}

/// Submit a request.
void
ircd::fs::aio::request::operator()()
{
	struct iocb *const cbs[]
	{
		static_cast<iocb *>(this)
	};

	assert(aioctx);
	syscall<SYS_io_submit>(aioctx->idp, 1, &cbs);
/*
	log::debug("AIO request(%p) fd:%u op:%d bytes:%lu off:%ld prio:%d ctx:%p submit",
	           this,
	           this->aio_fildes,
	           this->aio_lio_opcode,
	           this->aio_nbytes,
	           this->aio_offset,
	           this->aio_reqprio,
	           this->waiter);
*/
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
ircd::fs::aio::handle(const error_code &ec,
                      const size_t bytes)
noexcept
{
	assert((bytes == 8 && !ec && semval >= 1) || (bytes == 0 && ec));
	assert(!ec || ec.category() == asio::error::get_system_category());

	switch(ec.value())
	{
		case boost::system::errc::success:
			handle_events();
			break;

		case boost::system::errc::operation_canceled:
			return;

		default:
			throw boost::system::system_error(ec);
	}

	set_handle();
}

void
ircd::fs::aio::handle_events()
noexcept try
{
	std::array<io_event, MAX_EVENTS> event;

	// The number of completed requests available in events[]. This syscall
	// is restarted on EINTR. After restart, it may or may not find any ready
	// events but it never blocks to do so.
	const auto count
	{
		syscall_nointr<SYS_io_getevents>(idp, 0, event.size(), event.data(), nullptr)
	};

	// The count should be at least 1 event. The only reason to return 0 might
	// be related to an INTR; this assert will find out and may be commented.
	assert(count > 0);

	for(ssize_t i(0); i < count; ++i)
		handle_event(event[i]);
}
catch(const std::exception &e)
{
	log::error("AIO(%p) handle_events: %s",
	           this,
	           e.what());
}

void
ircd::fs::aio::handle_event(const io_event &event)
noexcept try
{
	// Our extended control block is passed in event.data
	auto *const request
	{
		reinterpret_cast<aio::request *>(event.data)
	};

	// The relevant iocb is repeated back to us in the result; we assert
	// some basic sanity here about the layout of the request conglomerate.
	assert(reinterpret_cast<iocb *>(event.obj) == static_cast<iocb *>(request));

	// error conventions are like so
	assert(event.res >= -1);  // unix syscall return value semantic
	assert(event.res2 >= 0);  // errno code semantic
	assert(event.res == -1 || event.res2 == 0);

	// Set result indicators
	request->retval = event.res;
	request->errcode = event.res2;
/*
	log::debug("AIO request(%p) fd:%d op:%d bytes:%lu off:%ld prio:%d ctx:%p result: bytes:%ld errno:%ld",
	           request,
	           request->aio_fildes,
	           request->aio_lio_opcode,
	           request->aio_nbytes,
	           request->aio_offset,
	           request->aio_reqprio,
	           request->waiter,
	           request->retval,
	           request->errcode);
*/
	const unwind cleanup{[&request]
	{
		// The user might want us to cleanup their fd after this event
		if(request->close_fd)
			request->close_fildes();

		// The user might want us to free a dynamically allocated request struct
		// to simplify their callback sequence. The noexcept guarantees h
		if(request->free_req)
			delete request;
	}};

	// virtual dispatch based on the request type. Alternatively, we could
	// switch() on the iocb lio_opcode and downcast... but that's what vtable
	// does for us right here.
	request->handle();
}
catch(const std::exception &e)
{
	log::critical("Unhandled request(%lu) event(%p) error: %s",
	              event.data,
	              &event,
	              e.what());
}

/// If requested, close the file descriptor. Errors here can be logged but
/// must be otherwise ignored.
void
ircd::fs::aio::request::close_fildes()
noexcept try
{
	syscall(::close, aio_fildes);
}
catch(const std::exception &e)
{
	log::error("Failed to close request(%p) fd:%d: %s",
	           this,
	           aio_fildes);
}

///////////////////////////////////////////////////////////////////////////////
//
// fs/read.h
//

namespace ircd::fs
{
	void read__aio(const string_view &path, const mutable_raw_buffer &, const read_opts &, read_callback);
	string_view read__aio(const string_view &path, const mutable_raw_buffer &, const read_opts &);
	std::string read__aio(const string_view &path, const read_opts &);
}

/// Read request control block
struct ircd::fs::aio::request::read
:request
{
	read_callback callback;

	virtual void handle() final override;

	read(const int &fd, const mutable_raw_buffer &buf, const read_opts &opts, read_callback callback);
};

ircd::fs::aio::request::read::read(const int &fd,
                                   const mutable_raw_buffer &buf,
                                   const read_opts &opts,
                                   read_callback callback)
:request{fd}
,callback{std::move(callback)}
{
	aio_data = uintptr_t(this);
	aio_reqprio = opts.priority;
	aio_lio_opcode = IOCB_CMD_PREAD;

	aio_buf = uintptr_t(buffer::data(buf));
	aio_nbytes = buffer::size(buf);
	aio_offset = opts.offset;
}

void
ircd::fs::aio::request::read::handle()
{
	if(waiter)
	{
		ircd::ctx::notify(*waiter);
		waiter = nullptr;
	}

	if(callback)
	{
		const size_t bytes
		{
			retval >= 0? size_t(retval) : 0
		};

		const string_view view
		{
			reinterpret_cast<const char *>(aio_buf), bytes
		};

		std::exception_ptr eptr
		{
			errcode != 0? make_system_error(errcode) : std::exception_ptr{}
		};

		callback(std::move(eptr), view);
	}
}

//
// Interface
//

std::string
ircd::fs::read__aio(const string_view &path,
                    const read_opts &opts)
{
	// Path to open(2) must be null terminated;
	static thread_local char pathstr[2048];
	strlcpy(pathstr, path, sizeof(pathstr));

	const auto fd
	{
		syscall(::open, pathstr, O_CLOEXEC, O_RDONLY)
	};

	const unwind cfd{[&fd]
	{
		syscall(::close, fd);
	}};

	// This fstat may be defeating; to be sure, don't use this overload
	// and progressively buffer chunks into your application.
	struct stat stat;
	syscall(::fstat, fd, &stat);
	const auto &size
	{
		stat.st_size
	};

	std::string ret(size, char{});
	const mutable_buffer buf
	{
		const_cast<char *>(ret.data()), ret.size()
	};

	aio::request::read request
	{
		int(fd), buf, opts, nullptr
	};

	const size_t bytes
	{
		request(ctx::cur())
	};

	ret.resize(bytes);
	return ret;
}

ircd::string_view
ircd::fs::read__aio(const string_view &path,
                    const mutable_raw_buffer &buf,
                    const read_opts &opts)
{
	// Path to open(2) must be null terminated;
	static thread_local char pathstr[2048];
	strlcpy(pathstr, path, sizeof(pathstr));

	const auto fd
	{
		syscall(::open, pathstr, O_CLOEXEC, O_RDONLY)
	};

	const unwind cfd{[&fd]
	{
		syscall(::close, fd);
	}};

	aio::request::read request
	{
		int(fd), buf, opts, nullptr
	};

	const size_t bytes
	{
		request(ctx::cur())
	};

	const string_view view
	{
		reinterpret_cast<const char *>(data(buf)), bytes
	};

	return view;
}

void
ircd::fs::read__aio(const string_view &path,
                    const mutable_raw_buffer &buf,
                    const read_opts &opts,
                    read_callback callback)
{
	static thread_local char pathstr[2048];
	strlcpy(pathstr, path, sizeof(pathstr));

	const auto fd
	{
		syscall(::open, pathstr, O_CLOEXEC, O_RDONLY)
	};

	const unwind::exceptional cfd{[&fd]
	{
		syscall(::close, fd);
	}};

	auto request
	{
		std::make_unique<aio::request::read>(int(fd), buf, opts, std::move(callback))
	};

	request->close_fd = true;
	request->free_req = true;
	(*request)();
	request.release();
}
