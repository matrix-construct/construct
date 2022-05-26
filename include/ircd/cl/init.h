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
#define HAVE_IRCD_CL_INIT_H

/// OpenCL Interface
namespace ircd::cl
{
	struct init;

	void log_dev_info(const uint platform_id, const uint device_id);
	void log_dev_info(const uint platform_id);
	void log_dev_info();

	void log_platform_info(const uint platform_id);
	void log_platform_info();
}

class [[gnu::visibility("hidden")]]
ircd::cl::init
{
	size_t init_platforms();
	size_t init_devices();
	size_t init_ctxs();
	bool init_libs();
	void fini_ctxs();
	void fini_libs();

  public:
	init(), ~init() noexcept;
};

#ifndef IRCD_USE_OPENCL
inline ircd::cl::init::init() {}
inline ircd::cl::init::~init() noexcept {}
#endif
