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
#define HAVE_AIO_H
#include <linux/aio_abi.h>

namespace ircd::fs::aio
{
	struct system;
	struct request;

	void prefetch(const fd &, const size_t &, const read_opts &);
	size_t write(const fd &, const const_iovec_view &, const write_opts &);
	size_t read(const fd &, const const_iovec_view &, const read_opts &);
	void fdsync(const fd &, const sync_opts &);
	void fsync(const fd &, const sync_opts &);
}

/// AIO context instance from the system. Right now this is a singleton with
/// an extern instance pointer at fs::aio::context maintained by fs::aio::init.
struct ircd::fs::aio::system
{
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

	/// An eventfd which will be notified by the system; we integrate this with
	/// the ircd io_service core epoll() event loop. The EFD_SEMAPHORE flag is
	/// not used to reduce the number of triggers. The semaphore value is the
	/// ecount (above) which will reflect a hint for how many AIO's are done.
	asio::posix::stream_descriptor resfd;

	/// Handler to the io context we submit requests to the system with
	aio_context_t idp {0};

	// Callback stack invoked when the sigfd is notified of completed events.
	void handle_event(const io_event &) noexcept;
	void handle_events() noexcept;
	void handle(const boost::system::error_code &, const size_t) noexcept;
	void set_handle();

	void flush() noexcept;
	void chase() noexcept;

	void submit(request &);
	void cancel(request &);
	void wait(request &);

	// Control panel
	bool wait();
	bool interrupt();

	system();
	~system() noexcept;
};

/// Generic request control block.
struct ircd::fs::aio::request
:iocb
{
	struct read;
	struct write;
	struct fdsync;
	struct fsync;

	ctx::ctx *waiter {ctx::current};
	ssize_t retval {std::numeric_limits<ssize_t>::min()};
	ssize_t errcode {0}; union
	{
		const read_opts *ropts {nullptr};
		const write_opts *wopts;
		const sync_opts *sopts;
	};

  public:
	const_iovec_view iovec() const;

	size_t operator()();
	void cancel();

	request(const int &fd);
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
