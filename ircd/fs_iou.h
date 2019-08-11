// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_FS_IOU_H
#include <linux/io_uring.h>

namespace ircd::fs::iou
{
	struct system;
	struct request;

	size_t write(const fd &, const const_iovec_view &, const write_opts &);
	size_t read(const fd &, const const_iovec_view &, const read_opts &);
	void fsync(const fd &, const sync_opts &);
}

struct ircd::fs::iou::system
{
	ctx::dock dock;

	::io_uring_params p;
	fs::fd fd;
	size_t sq_len, cq_len, sqe_len;
	custom_ptr<uint8_t> sq_p, cq_p, sqe_p;
	uint32_t *head[2];
	uint32_t *tail[2];
	uint32_t *ring_mask[2];
	uint32_t *ring_entries[2];
	uint32_t *flags[2];
	uint32_t *dropped[2];
	uint32_t *overflow[2];
	uint32_t *sq;
	::io_uring_sqe *sqe;
	::io_uring_cqe *cqe;

	size_t ev_count;
	asio::posix::stream_descriptor ev_fd;
	bool handle_set;
	size_t handle_size;
	std::unique_ptr<uint8_t[]> handle_data;
	static ios::descriptor handle_descriptor;

	void handle_events() noexcept;
	void handle(const boost::system::error_code &ec, const size_t bytes) noexcept;
	void set_handle();

	bool interrupt();
	bool wait();

	system(const size_t &max_events,
	       const size_t &max_submit);

	~system() noexcept;
};
