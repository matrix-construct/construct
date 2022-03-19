// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_CL_DATA_H

/// cl_mem wrapping
struct ircd::cl::data
{
	static conf::item<size_t> gart_page_size;

	void *handle {nullptr};
	void *mapped {nullptr};

  public:
	uint flags() const;
	size_t size() const;
	off_t offset() const;
	size_t refs() const;
	char *ptr() const;

	operator const_buffer() const;
	operator mutable_buffer() const;

	data(const size_t, const bool host_rd = false, const bool host_wr = false);
	data(const mutable_buffer &, const bool dev_wonly = false); // host rw
	data(const const_buffer &); // host ro
	data(data &, const pair<size_t, off_t> &); // subbuffer
	data(const data &) = delete;
	data() = default;
	data(data &&) noexcept;
	data &operator=(const data &) = delete;
	data &operator=(data &&) noexcept;
	~data() noexcept;
};
