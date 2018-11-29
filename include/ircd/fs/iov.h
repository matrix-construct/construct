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
#define HAVE_IRCD_FS_IOV_H

extern "C"
{
	struct iovec;
};

namespace ircd::fs
{
	// utility; count the total bytes of an iov.
	size_t bytes(const vector_view<const struct ::iovec> &);

	// Transforms our buffers to struct iovec ones. This is done using an
	// internal thread_local array of IOV_MAX. The returned view is of that
	// array. We get away with using a single buffer because the synchronous
	// readv()/writev() calls block the thread and for AIO the iov is copied out
	// of userspace on io_submit().
	vector_view<const struct ::iovec> make_iov(const vector_view<const const_buffer> &);
	vector_view<const struct ::iovec> make_iov(const vector_view<const mutable_buffer> &);
}
