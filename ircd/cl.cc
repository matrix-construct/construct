// The Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <CL/cl.h>

// Util
namespace ircd::cl
{
	static bool is_error(const int &code) noexcept;
	static int throw_on_error(const int &code);
	template<class func, class... args> static int call(func&&, args&&...);
	template<class T = string_view, class F, class id, class param> static T info(F&&, const id &, const param &, const mutable_buffer &);
}

// Runtime state
namespace ircd::cl
{
	static const size_t
	PLATFORM_MAX {8},
	DEVICE_MAX {8};

	static uint
	platforms,
	devices[PLATFORM_MAX];

	static cl_platform_id
	platform[PLATFORM_MAX];

	static cl_device_id
	device[PLATFORM_MAX][DEVICE_MAX];

	static cl_context
	primary;

	static cl_command_queue
	queue[PLATFORM_MAX][DEVICE_MAX];

	static void handle_notify(const char *, const void *, size_t, void *) noexcept;
}

decltype(ircd::cl::log)
ircd::cl::log
{
	"cl"
};

decltype(ircd::cl::version_api)
ircd::cl::version_api
{
	"OpenCL", info::versions::API, CL_TARGET_OPENCL_VERSION,
	{
		#if defined(CL_VERSION_MAJOR) && defined(CL_VERSION_MINOR) && defined(CL_VERSION_PATCH)
		CL_VERSION_MAJOR(CL_TARGET_OPENCL_VERSION),
		CL_VERSION_MINOR(CL_TARGET_OPENCL_VERSION),
		CL_VERSION_PATCH(CL_TARGET_OPENCL_VERSION),
		#endif
	}
};

decltype(ircd::cl::version_abi)
ircd::cl::version_abi
{
	"OpenCL", info::versions::ABI, 0,
	{
		0, 0, 0
	}
};

//
// init
//

ircd::cl::init::init()
{
	call(clGetPlatformIDs, PLATFORM_MAX, platform, &platforms);

	char buf[4][128];
	for(size_t i(0); i < platforms; ++i)
		log::info
		{
			log, "OpenCL:%d [%u][*] :%s :%s :%s :%s",
			CL_TARGET_OPENCL_VERSION,
			i,
			info(clGetPlatformInfo, platform[i], CL_PLATFORM_VERSION, buf[0]),
			info(clGetPlatformInfo, platform[i], CL_PLATFORM_VENDOR, buf[1]),
			info(clGetPlatformInfo, platform[i], CL_PLATFORM_NAME, buf[2]),
			info(clGetPlatformInfo, platform[i], CL_PLATFORM_EXTENSIONS, buf[3]),
		};

	size_t devices_total(0);
	for(size_t i(0); i < platforms; ++i)
	{
		static const auto type
		{
			CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_ACCELERATOR
		};

		call(clGetDeviceIDs, platform[i], type, DEVICE_MAX, device[i], devices + i);
		devices_total += devices[i];
	}

	for(size_t i(0); i < platforms; ++i)
		for(size_t j(0); j < devices[i]; ++j)
			log::info
			{
				log, "OpenCL:%d [%u][%u] :%s :%s :%s :%s",
				CL_TARGET_OPENCL_VERSION,
				i,
				j,
				info(clGetDeviceInfo, device[i][j], CL_DRIVER_VERSION, buf[0]),
				info(clGetDeviceInfo, device[i][j], CL_DEVICE_VERSION, buf[1]),
				info(clGetDeviceInfo, device[i][j], CL_DEVICE_VENDOR, buf[2]),
				info(clGetDeviceInfo, device[i][j], CL_DEVICE_NAME, buf[3]),
			};

	// Gather all devices we'll use.
	size_t _devs {0};
	cl_device_id _dev[DEVICE_MAX];
	for(size_t i(0); i < platforms; ++i)
		for(size_t j(0); j < devices[i]; ++j)
			_dev[_devs++] = device[i][j];

	// Create a context from gathered devices.
	cl_int err {CL_SUCCESS};
	cl_context_properties ctxprop {0};
	primary = clCreateContext(&ctxprop, _devs, _dev, handle_notify, nullptr, &err);
	throw_on_error(err);

	// Create a queue for each device.
	cl_queue_properties qprop {0};
	for(size_t i(0); i < platforms; ++i)
		for(size_t j(0); j < devices[i]; ++j)
		{
			queue[i][j] = clCreateCommandQueueWithProperties(primary, device[i][j], &qprop, &err);
			throw_on_error(err);
		}
}

ircd::cl::init::~init()
noexcept
{
	if(primary)
		log::debug
		{
			log, "Shutting down OpenCL...",
		};

	for(size_t i(0); i < PLATFORM_MAX; ++i)
		for(size_t j(0); j < DEVICE_MAX; ++j)
			if(queue[i][j])
			{
				call(clReleaseCommandQueue, queue[i][j]);
				queue[i][j] = nullptr;
			}

	if(primary)
	{
		call(clReleaseContext, primary);
		primary = nullptr;
	}
}

//
// callback surface
//

void
ircd::cl::handle_notify(const char *errstr,
                        const void *token,
                        size_t cb,
                        void *priv)
noexcept
{
	if(errstr)
		log::error
		{
			log, "OpenCL t:%p cb:%zu :%s",
			token,
			cb,
			errstr,
		};
}

//
// util
//

template<class T,
         class F,
         class id,
         class param>
T
ircd::cl::info(F&& func,
               const id &i,
               const param &p,
               const mutable_buffer &out)
{
	size_t len {0};
	call(std::forward<F>(func), i, p, size(out), data(out), &len);
	const string_view str
	{
		data(out), len
	};

	return lex_cast<T>(str);
}

template<class func,
         class... args>
int
ircd::cl::call(func&& f,
               args&&... a)
{
	const int ret
	{
		f(std::forward<args>(a)...)
	};

	return throw_on_error(ret);
}

int
ircd::cl::throw_on_error(const int &code)
{
	if(unlikely(is_error(code)))
		throw opencl_error
		{
			"(#%d) :%s",
			code,
			reflect_error(code),
		};

	return code;
}

bool
ircd::cl::is_error(const int &code)
noexcept
{
	return code < 0;
}

ircd::string_view
ircd::cl::reflect_error(const int code)
noexcept
{
	switch(code)
	{
		case CL_SUCCESS:                           return "SUCCESS";
		case CL_DEVICE_NOT_FOUND:                  return "DEVICE_NOT_FOUND";
		case CL_DEVICE_NOT_AVAILABLE:              return "DEVICE_NOT_AVAILABLE";
		case CL_COMPILER_NOT_AVAILABLE:            return "COMPILER_NOT_AVAILABLE";
		case CL_MEM_OBJECT_ALLOCATION_FAILURE:     return "MEM_OBJECT_ALLOCATION_FAILURE";
		case CL_OUT_OF_RESOURCES:                  return "OUT_OF_RESOURCES";
		case CL_OUT_OF_HOST_MEMORY:                return "OUT_OF_HOST_MEMORY";
		case CL_PROFILING_INFO_NOT_AVAILABLE:      return "PROFILING_INFO_NOT_AVAILABLE";
		case CL_MEM_COPY_OVERLAP:                  return "MEM_COPY_OVERLAP";
		case CL_IMAGE_FORMAT_MISMATCH:             return "IMAGE_FORMAT_MISMATCH";
		case CL_IMAGE_FORMAT_NOT_SUPPORTED:        return "IMAGE_FORMAT_NOT_SUPPORTED";
		case CL_BUILD_PROGRAM_FAILURE:             return "BUILD_PROGRAM_FAILURE";
		case CL_MAP_FAILURE:                       return "MAP_FAILURE";
		case CL_INVALID_VALUE:                     return "INVALID_VALUE";
		case CL_INVALID_DEVICE_TYPE:               return "INVALID_DEVICE_TYPE";
		case CL_INVALID_PLATFORM:                  return "INVALID_PLATFORM";
		case CL_INVALID_DEVICE:                    return "INVALID_DEVICE";
		case CL_INVALID_CONTEXT:                   return "INVALID_CONTEXT";
		case CL_INVALID_QUEUE_PROPERTIES:          return "INVALID_QUEUE_PROPERTIES";
		case CL_INVALID_COMMAND_QUEUE:             return "INVALID_COMMAND_QUEUE";
		case CL_INVALID_HOST_PTR:                  return "INVALID_HOST_PTR";
		case CL_INVALID_MEM_OBJECT:                return "INVALID_MEM_OBJECT";
		case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:   return "INVALID_IMAGE_FORMAT_DESCRIPTOR";
		case CL_INVALID_IMAGE_SIZE:                return "INVALID_IMAGE_SIZE";
		case CL_INVALID_SAMPLER:                   return "INVALID_SAMPLER";
		case CL_INVALID_BINARY:                    return "INVALID_BINARY";
		case CL_INVALID_BUILD_OPTIONS:             return "INVALID_BUILD_OPTIONS";
		case CL_INVALID_PROGRAM:                   return "INVALID_PROGRAM";
		case CL_INVALID_PROGRAM_EXECUTABLE:        return "INVALID_PROGRAM_EXECUTABLE";
		case CL_INVALID_KERNEL_NAME:               return "INVALID_KERNEL_NAME";
		case CL_INVALID_KERNEL_DEFINITION:         return "INVALID_KERNEL_DEFINITION";
		case CL_INVALID_KERNEL:                    return "INVALID_KERNEL";
		case CL_INVALID_ARG_INDEX:                 return "INVALID_ARG_INDEX";
		case CL_INVALID_ARG_VALUE:                 return "INVALID_ARG_VALUE";
		case CL_INVALID_ARG_SIZE:                  return "INVALID_ARG_SIZE";
		case CL_INVALID_KERNEL_ARGS:               return "INVALID_KERNEL_ARGS";
		case CL_INVALID_WORK_DIMENSION:            return "INVALID_WORK_DIMENSION";
		case CL_INVALID_WORK_GROUP_SIZE:           return "INVALID_WORK_GROUP_SIZE";
		case CL_INVALID_WORK_ITEM_SIZE:            return "INVALID_WORK_ITEM_SIZE";
		case CL_INVALID_GLOBAL_OFFSET:             return "INVALID_GLOBAL_OFFSET";
		case CL_INVALID_EVENT_WAIT_LIST:           return "INVALID_EVENT_WAIT_LIST";
		case CL_INVALID_EVENT:                     return "INVALID_EVENT";
		case CL_INVALID_OPERATION:                 return "INVALID_OPERATION";
		case CL_INVALID_GL_OBJECT:                 return "INVALID_GL_OBJECT";
		case CL_INVALID_BUFFER_SIZE:               return "INVALID_BUFFER_SIZE";
		case CL_INVALID_MIP_LEVEL:                 return "INVALID_MIP_LEVEL";
		case CL_INVALID_GLOBAL_WORK_SIZE:          return "INVALID_GLOBAL_WORK_SIZE";

		#ifdef CL_VERSION_1_1
		case CL_INVALID_PROPERTY:                  return "INVALID_PROPERTY";
		case CL_MISALIGNED_SUB_BUFFER_OFFSET:      return "MISALIGNED_SUB_BUFFER_OFFSET";
		case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST: return "EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
		#endif

		#ifdef CL_VERSION_1_2
		case CL_COMPILE_PROGRAM_FAILURE:           return "COMPILE_PROGRAM_FAILURE";
		case CL_LINKER_NOT_AVAILABLE:              return "LINKER_NOT_AVAILABLE";
		case CL_LINK_PROGRAM_FAILURE:              return "LINK_PROGRAM_FAILURE";
		case CL_DEVICE_PARTITION_FAILED:           return "DEVICE_PARTITION_FAILED";
		case CL_KERNEL_ARG_INFO_NOT_AVAILABLE:     return "KERNEL_ARG_INFO_NOT_AVAILABLE";
		case CL_INVALID_IMAGE_DESCRIPTOR:          return "INVALID_IMAGE_DESCRIPTOR";
		case CL_INVALID_COMPILER_OPTIONS:          return "INVALID_COMPILER_OPTIONS";
		case CL_INVALID_LINKER_OPTIONS:            return "INVALID_LINKER_OPTIONS";
		case CL_INVALID_DEVICE_PARTITION_COUNT:    return "INVALID_DEVICE_PARTITION_COUNT";
		#endif

		#ifdef CL_VERSION_2_0
		case CL_INVALID_PIPE_SIZE:                 return "INVALID_PIPE_SIZE";
		case CL_INVALID_DEVICE_QUEUE:              return "INVALID_DEVICE_QUEUE";
		#endif

		#ifdef CL_VERSION_2_2
		case CL_INVALID_SPEC_ID:                   return "INVALID_SPEC_ID";
		case CL_MAX_SIZE_RESTRICTION_EXCEEDED:     return "MAX_SIZE_RESTRICTION_EXCEEDED";
		#endif
	}

	return "???????";
}
