// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_FS_AIO_H
#include <linux/aio_abi.h>

/// Define this to try all read requests with RWF_NOWAIT first and indicate
/// EAGAIN failure with a log dwarning. On EAGAIN the request is resubmitted
/// without RWF_NOWAIT if the user expected blocking behavior.
//#define RB_DEBUG_FS_AIO_READ_BLOCKING

/// Define to time the io_submit() system call with a syscall_usage_warning.
/// This emits a warning when the kernel spent a lot of time in io_submit().
/// The resolution is very low at 10ms but it is not expensive to use.
//#define RB_DEBUG_FS_AIO_SUBMIT_BLOCKING

namespace ircd::fs::aio
{
	struct system;
	struct request;

	size_t write(const fd &, const const_iovec_view &, const write_opts &);
	size_t read(const fd &, const const_iovec_view &, const read_opts &);
	void fdsync(const fd &, const sync_opts &);
	void fsync(const fd &, const sync_opts &);
}

/// AIO context instance from the system. Right now this is a singleton with
/// an extern instance pointer at fs::aio::context maintained by fs::aio::init.
struct ircd::fs::aio::system
{
	struct aio_context;

	static const int eventfd_flags;

	/// io_getevents vector (in)
	std::vector<io_event> event;
	uint64_t ecount {0};

	/// io_submit queue (out)
	std::vector<iocb *> queue;
	size_t qcount {0};

	/// other state
	ctx::dock dock;
	size_t in_flight {0};
	bool handle_set {false};

	size_t handle_size {0};
	std::unique_ptr<uint8_t[]> handle_data;
	static ios::descriptor handle_descriptor;

	/// An eventfd which will be notified by the system; we integrate this with
	/// the ircd io_service core epoll() event loop. The EFD_SEMAPHORE flag is
	/// not used to reduce the number of triggers. The semaphore value is the
	/// ecount (above) which will reflect a hint for how many AIO's are done.
	asio::posix::stream_descriptor resfd;

	/// Handler to the io context we submit requests to the system with
	const custom_ptr<const aio_context> head;
	const io_event *ring {nullptr};

	size_t max_events() const;
	size_t max_submit() const;
	size_t request_count() const; // qcount + in_flight
	size_t request_avail() const; // max_events - request_count()

	// Callback stack invoked when the sigfd is notified of completed events.
	void handle_event(const io_event &) noexcept;
	void handle_events() noexcept;
	void handle(const boost::system::error_code &, const size_t) noexcept;
	void set_handle();

	void dequeue_one(const std::error_code &);
	void dequeue_all(const std::error_code &);
	size_t io_submit();
	size_t submit() noexcept;
	void chase() noexcept;

	bool submit(request &);
	bool cancel(request &);
	bool wait(request &);

	// Control panel
	bool wait();
	bool interrupt();

	system(const size_t &max_events,
	       const size_t &max_submit);

	~system() noexcept;
};

struct ircd::fs::aio::system::aio_context
{
	static constexpr uint MAGIC {0xA10A10A1};

	uint id; // kernel internal index number
	uint nr; // number of io_events
	uint head;
	uint tail;
	uint magic;
	uint compat_features;
	uint incompat_features;
	uint header_length;  // size of aio_context
	struct io_event io_events[0];
};
// 128 bytes + ring size

/// Generic request control block.
struct ircd::fs::aio::request
:iocb
{
	struct read;
	struct write;
	struct fdsync;
	struct fsync;

	ssize_t retval {-2L};
	ssize_t errcode {0L};
	const struct opts *opts {nullptr};
	ctx::ctx *waiter {ctx::current};

  public:
	const_iovec_view iovec() const;
	bool completed() const;
	bool queued() const;

	size_t operator()();
	bool cancel();

	request(const int &fd, const struct opts *const &);
	~request() noexcept;
};

/// Read request control block
struct ircd::fs::aio::request::read
:request
{
	read(const int &fd, const const_iovec_view &, const read_opts &);
};

/// Write request control block
struct ircd::fs::aio::request::write
:request
{
	write(const int &fd, const const_iovec_view &, const write_opts &);
};

/// fdsync request control block
struct ircd::fs::aio::request::fdsync
:request
{
	fdsync(const int &fd, const sync_opts &);
};

/// fsync request control block
struct ircd::fs::aio::request::fsync
:request
{
	fsync(const int &fd, const sync_opts &);
};
