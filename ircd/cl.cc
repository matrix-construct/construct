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
		log::logf
		{
			log, log::level::DEBUG,
			"OpenCL:%d [%u][*] :%s :%s :%s :%s",
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
				info(clGetDeviceInfo, device[i][j], CL_DEVICE_VERSION, buf[1]),
				info(clGetDeviceInfo, device[i][j], CL_DEVICE_VENDOR, buf[2]),
				info(clGetDeviceInfo, device[i][j], CL_DEVICE_NAME, buf[3]),
				info(clGetDeviceInfo, device[i][j], CL_DRIVER_VERSION, buf[0]),
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
	cl_command_queue_properties qprop {0};
	qprop |= CL_QUEUE_PROFILING_ENABLE;
	for(size_t i(0); i < platforms; ++i)
		for(size_t j(0); j < devices[i]; ++j)
		{
			queue[i][j] = clCreateCommandQueue(primary, device[i][j], qprop, &err);
			throw_on_error(err);
		}
}

ircd::cl::init::~init()
noexcept
{
	if(primary)
	{
		log::debug
		{
			log, "Shutting down OpenCL...",
		};

		sync();
	}

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
// interface
//

void
ircd::cl::sync()
{
	auto &q
	{
		queue[0][0]
	};

	call
	(
		clFinish, q
	);
}

void
ircd::cl::flush()
{
	auto &q
	{
		queue[0][0]
	};

	call
	(
		clFlush, q
	);
}

//
// exec
//

ircd::cl::exec::exec(kern &kern,
                     const kern::range &work)
try
{
	const auto &handle
	{
		reinterpret_cast<cl_kernel>(kern.handle)
	};

	size_t dim(0);
	for(size_t i(0); i < work.global.size(); ++i)
		dim += work.global[i] > 0;

	size_t dependencies {0};
	cl_event *const dependency
	{
		nullptr
	};

	auto &q
	{
		queue[0][0]
	};

	call
	(
		clEnqueueNDRangeKernel,
		q,
		handle,
		dim,
		work.offset.data(),
		work.global.data(),
		work.local.data(),
		dependencies,
		dependency,
		reinterpret_cast<cl_event *>(&this->handle)
	);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Exec Kern :%s",
		e.what(),
	};

	throw;
}

ircd::cl::exec::exec(data &data,
                     const mutable_buffer &buf,
                     const bool blocking)
try
{
	const auto &handle
	{
		reinterpret_cast<cl_mem>(data.handle)
	};

	size_t dependencies {0};
	cl_event *const dependency
	{
		nullptr
	};

	auto &q
	{
		queue[0][0]
	};

	call
	(
		clEnqueueReadBuffer,
		q,
		handle,
		blocking,
		0UL, //offset,
		ircd::size(buf),
		ircd::data(buf),
		dependencies,
		dependency,
		reinterpret_cast<cl_event *>(&this->handle)
	);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Exec Read :%s",
		e.what(),
	};

	throw;
}

ircd::cl::exec::exec(data &data,
                     const const_buffer &buf,
                     const bool blocking)
try
{
	const auto &handle
	{
		reinterpret_cast<cl_mem>(data.handle)
	};

	size_t dependencies {0};
	cl_event *const dependency
	{
		nullptr
	};

	auto &q
	{
		queue[0][0]
	};

	call
	(
		clEnqueueReadBuffer,
		q,
		handle,
		blocking,
		0UL, //offset,
		ircd::size(buf),
		mutable_cast(ircd::data(buf)),
		dependencies,
		dependency,
		reinterpret_cast<cl_event *>(&this->handle)
	);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Exec Write :%s",
		e.what(),
	};

	throw;
}

//
// kern
//

ircd::cl::kern::kern(code &code,
                     const string_view &name)
try
{
	const auto &program
	{
		reinterpret_cast<cl_program>(code.handle)
	};

	int err {CL_SUCCESS};
	handle = clCreateKernel(program, name.c_str(), &err);
	throw_on_error(err);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Kernel Create '%s' :%s",
		name,
		e.what(),
	};

	throw;
}

ircd::cl::kern::kern(kern &&o)
noexcept
:handle{std::move(o.handle)}
{
	o.handle = nullptr;
}

ircd::cl::kern &
ircd::cl::kern::operator=(kern &&o)
noexcept
{
	this->~kern();
	handle = std::move(o.handle);
	o.handle = nullptr;
	return *this;
}

ircd::cl::kern::~kern()
noexcept try
{
	call(clReleaseKernel, reinterpret_cast<cl_kernel>(handle));
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Kernel Release :%s",
		e.what(),
	};

	return;
}

void
ircd::cl::kern::arg(const int i,
                    data &data)
{
	const auto &handle
	{
		reinterpret_cast<cl_kernel>(this->handle)
	};

	const auto &arg_handle
	{
		reinterpret_cast<cl_mem>(data.handle)
	};

	call(clSetKernelArg, handle, i, sizeof(cl_mem), &arg_handle);
}

//
// code
//

ircd::cl::code::code(const string_view &src)
:code
{
	vector_view<const string_view>(&src, 1)
}
{
}

ircd::cl::code::code(const vector_view<const string_view> &srcs)
{
	static const size_t iov_max
	{
		64 //TODO: ???
	};

	if(unlikely(srcs.size() > iov_max))
		throw error
		{
			"Maximum number of sources exceeded: lim:%zu got:%zu",
			iov_max,
			srcs.size(),
		};

	const size_t count
	{
		std::min(srcs.size(), iov_max)
	};

	size_t len[count];
	const char *src[count];
	for(size_t i(0); i < count; ++i)
		src[i] = ircd::data(srcs[i]),
		len[i] = ircd::size(srcs[i]);

	int err {CL_SUCCESS};
	handle = clCreateProgramWithSource(primary, count, src, len, &err);
	throw_on_error(err);
}

ircd::cl::code::code(code &&o)
noexcept
:handle{std::move(o.handle)}
{
	o.handle = nullptr;
}

ircd::cl::code &
ircd::cl::code::operator=(code &&o)
noexcept
{
	this->~code();
	handle = std::move(o.handle);
	o.handle = nullptr;
	return *this;
}

ircd::cl::code::~code()
noexcept try
{
	call(clReleaseProgram, reinterpret_cast<cl_program>(handle));
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Program Release :%s",
		e.what(),
	};

	return;
}

namespace ircd::cl
{
	static void
	handle_built(cl_program program, void *priv)
	{
		ircd::always_assert(false);
	}
}

void
ircd::cl::code::build(const string_view &opts)
try
{
	const auto &handle
	{
		reinterpret_cast<cl_program>(this->handle)
	};

	const uint num_devices {0};
	const cl_device_id *const device_list {nullptr};
	call
	(
		clBuildProgram,
		handle,
		num_devices,
		device_list,
		opts.c_str(),
		&cl::handle_built,
		nullptr
	);
}
catch(const std::exception &e)
{
	const auto error_closure{[this]
	(const mutable_buffer &buf)
	{
		size_t len {0}; call
		(
			clGetProgramBuildInfo,
			reinterpret_cast<cl_program>(this->handle),
			device[0][0],
			CL_PROGRAM_BUILD_LOG,
			ircd::size(buf),
			ircd::data(buf),
			&len
		);

		return len;
	}};

	const auto error_message
	{
		ircd::string(8_KiB | SHRINK_TO_FIT, error_closure)
	};

	ircd::tokens(error_message, '\n', []
	(const string_view &line)
	{
		// note last line is just a CR
		if(likely(line.size() > 1))
			log::logf
			{
				log, log::DERROR, "%s", line,
			};
	});

	throw;
}

//
// data::mmap
//

ircd::cl::data::mmap::mmap(data &data,
                           const size_t size,
                           const bool write,
                           const bool writeonly)
try
:memory{&data}
{
	const auto &handle
	{
		reinterpret_cast<cl_mem>(data.handle)
	};

	size_t dependencies {0};
	cl_event *const dependency
	{
		nullptr
	};

	auto &q
	{
		queue[0][0]
	};

	cl_map_flags flags {0};
	flags |= write? CL_MAP_WRITE: 0;
	flags |= !writeonly? CL_MAP_READ: 0;

	int err {CL_SUCCESS};
	void *const map
	{
		clEnqueueMapBuffer
		(
			q,
			handle,
			true, // blocking,
			flags,
			0UL, // offset,
			size,
			dependencies,
			dependency,
			nullptr,
			&err
		)
	};

	throw_on_error(err);
	static_cast<mutable_buffer &>(*this) = mutable_buffer
	{
		reinterpret_cast<char *>(map), size
	};
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Push Mmap :%s",
		e.what(),
	};

	throw;
}

ircd::cl::data::mmap::mmap(mmap &&o)
noexcept
:mutable_buffer{std::move(o)}
,memory{std::move(o.memory)}
{
	std::get<0>(o) = nullptr;
	std::get<1>(o) = nullptr;
	o.memory = nullptr;
}

ircd::cl::data::mmap &
ircd::cl::data::mmap::operator=(mmap &&o)
noexcept
{
	this->~mmap();
	static_cast<mutable_buffer &>(*this) = std::move(o);
	memory = std::move(o.memory);
	std::get<0>(o) = nullptr;
	std::get<1>(o) = nullptr;
	o.memory = nullptr;
	return *this;
}

ircd::cl::data::mmap::~mmap()
noexcept try
{
	if(!std::get<0>(*this))
		return;

	assert(!memory || memory->handle);
	if(!memory || !memory->handle)
		return;

	size_t dependencies {0};
	cl_event *const dependency
	{
		nullptr
	};

	auto &q
	{
		queue[0][0]
	};

	call
	(
		clEnqueueUnmapMemObject,
		q,
		reinterpret_cast<cl_mem>(memory->handle),
		std::get<0>(*this),
		dependencies,
		dependency,
		nullptr
	);

	//TODO: replace with better waiter
	cl::sync();
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Mmap Release :%s",
		e.what(),
	};

	return;
}

//
// data
//

ircd::cl::data::data(const size_t size,
                     const bool w,
                     const bool wonly)
{
	int err {CL_SUCCESS};
	cl_mem_flags flags {0};
	flags |= wonly? CL_MEM_WRITE_ONLY: 0;
	flags |= !w? CL_MEM_READ_ONLY: 0;
	handle = clCreateBuffer(primary, flags, size, nullptr, &err);
	throw_on_error(err);
}

ircd::cl::data::data(const mutable_buffer &buf,
                     const bool wonly)
{
	int err {CL_SUCCESS};
	cl_mem_flags flags {0};
	flags |= CL_MEM_USE_HOST_PTR;
	flags |= wonly? CL_MEM_WRITE_ONLY: CL_MEM_READ_WRITE;
	handle = clCreateBuffer(primary, flags, ircd::size(buf), ircd::data(buf), &err);
	throw_on_error(err);
}

ircd::cl::data::data(const const_buffer &buf)
{
	int err {CL_SUCCESS};
	cl_mem_flags flags {0};
	flags |= CL_MEM_USE_HOST_PTR;
	flags |= CL_MEM_READ_ONLY;
	handle = clCreateBuffer(primary, flags, ircd::size(buf), mutable_cast(ircd::data(buf)), &err);
	throw_on_error(err);
}

ircd::cl::data::data(data &&o)
noexcept
:handle{std::move(o.handle)}
{
	o.handle = nullptr;
}

ircd::cl::data &
ircd::cl::data::operator=(data &&o)
noexcept
{
	this->~data();
	handle = std::move(o.handle);
	o.handle = nullptr;
	return *this;
}

ircd::cl::data::~data()
noexcept try
{
	call(clReleaseMemObject, reinterpret_cast<cl_mem>(handle));
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Memory Release :%s",
		e.what(),
	};

	return;
}

//
// cl::work (event)
//

namespace ircd::cl
{
	struct handle_event_data
	{
		ctx::ctx *c {ctx::current};
	};

	static void handle_event(cl_event, cl_int, void *) noexcept;
}

//
// work::work
//

ircd::cl::work::work(void *const &handle)
{
	call(clRetainEvent, cl_event(handle));
	this->handle = handle;
}

ircd::cl::work::~work()
noexcept try
{
	const auto handle
	{
		reinterpret_cast<cl_event>(this->handle)
	};

	if(likely(handle))
	{
		struct handle_event_data hdata;
		call(clSetEventCallback, handle, CL_COMPLETE, &cl::handle_event, &hdata);

		char status_buf[8] {0};
		const auto &status
		{
			info<int>(clGetEventInfo, handle, CL_EVENT_COMMAND_EXECUTION_STATUS, status_buf)
		};

		if(status != CL_COMPLETE)
		{
			const ctx::uninterruptible::nothrow ui;
			while(hdata.c)
			{
				ctx::wait();
				std::atomic_thread_fence(std::memory_order_acquire);
			}
		}

		call(clReleaseEvent, reinterpret_cast<cl_event>(handle));
	}
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Work Release :%s",
		e.what(),
	};

	return;
}

std::array<uint64_t, 4>
ircd::cl::work::profile()
const
{
	const auto handle
	{
		reinterpret_cast<cl_event>(this->handle)
	};

	char buf[4][8];
	return std::array<uint64_t, 4>
	{
		info<size_t>(clGetEventProfilingInfo, handle, CL_PROFILING_COMMAND_QUEUED, buf[0]),
		info<size_t>(clGetEventProfilingInfo, handle, CL_PROFILING_COMMAND_SUBMIT, buf[1]),
		info<size_t>(clGetEventProfilingInfo, handle, CL_PROFILING_COMMAND_START, buf[2]),
		info<size_t>(clGetEventProfilingInfo, handle, CL_PROFILING_COMMAND_END, buf[3]),
	};
}

void
ircd::cl::handle_event(cl_event event,
                       cl_int status,
                       void *const priv)
noexcept
{
	auto hdata
	{
		reinterpret_cast<handle_event_data *>(priv)
	};

	const auto c
	{
		std::exchange(hdata->c, nullptr)
	};

	ctx::notify(*c);
	std::atomic_thread_fence(std::memory_order_release);
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
	using ircd::data;
	using ircd::size;

	size_t len {0};
	call(std::forward<F>(func), i, p, size(out), data(out), &len);
	const string_view str
	{
		data(out), len
	};

	return byte_view<T>(str);
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
