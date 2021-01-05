// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_CL_H

/// OpenCL Interface
namespace ircd::cl
{
	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, opencl_error)

	struct init;

	extern log::log log;
	extern const info::versions version_api, version_abi;

	string_view reflect_error(const int code) noexcept;
}

struct ircd::cl::init
{
	init();
	~init() noexcept;
};

#ifndef IRCD_USE_OPENCL
inline
ircd::cl::init::init()
{}
#endif

#ifndef IRCD_USE_OPENCL
inline
ircd::cl::init::~init()
noexcept
{}
#endif
