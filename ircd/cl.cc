// The Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/ircd.h>
#include <CL/cl.h>

namespace ircd::cl
{
	extern const info::versions opencl_version_api, opencl_version_abi;
}

decltype(ircd::cl::opencl_version_api)
ircd::cl::opencl_version_api
{
	"OpenCL", info::versions::API, CL_TARGET_OPENCL_VERSION,
	{
	    #if defined(CL_VERSION_MAJOR)
        CL_VERSION_MAJOR(CL_TARGET_OPENCL_VERSION),
        CL_VERSION_MINOR(CL_TARGET_OPENCL_VERSION),
        CL_VERSION_PATCH(CL_TARGET_OPENCL_VERSION),
        #endif
	}
};

decltype(ircd::cl::opencl_version_abi)
ircd::cl::opencl_version_abi
{
	"OpenCL", info::versions::ABI, 0,
	{
        0, 0, 0
	}
};
