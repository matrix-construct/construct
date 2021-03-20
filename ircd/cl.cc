// The Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <dlfcn.h>
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
	OPTION_MAX {8},
	PLATFORM_MAX {8},
	DEVICE_MAX {8};

	static uint
	options,
	platforms,
	devices[PLATFORM_MAX];

	static char
	option[OPTION_MAX][256];

	static void
	*linkage;

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
	"OpenCL", info::versions::ABI
};

decltype(ircd::cl::enable)
ircd::cl::enable
{
	{ "name",      "ircd.cl.enable"  },
	{ "default",   false             },
	{ "persist",   false             },
};

decltype(ircd::cl::profile_queue)
ircd::cl::profile_queue
{
	{ "name",      "ircd.cl.profile.queue"  },
	{ "default",   false                    },
	{ "persist",   false                    },
};

//
// init
//

ircd::cl::init::init()
{
	if(!enable)
	{
		log::dwarning
		{
			log, "OpenCL hardware acceleration is not available or enabled."
		};

		return;
	}

	const ctx::posix::enable_pthread enable_pthread;

	// Setup options
	strlcpy{option[options++], "LP_NUM_THREADS=0"};
	strlcpy{option[options++], "MESA_GLSL_CACHE_DISABLE=true"};
	strlcpy{option[options++], "AMD_DEBUG=nogfx"};
	assert(options <= OPTION_MAX);

	// Configure options into the environment. TODO: XXX don't overwrite
	while(options--)
		sys::call(putenv, option[options]);

	// Load the pipe.
	assert(!linkage);
	if(!(linkage = dlopen("libOpenCL.so", RTLD_LAZY | RTLD_GLOBAL)))
		return;

	// OpenCL sez platform=null is implementation defined.
	info(clGetPlatformInfo, nullptr, CL_PLATFORM_VERSION, version_abi.string);

	// Get the platforms.
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
	qprop |= (profile_queue? CL_QUEUE_PROFILING_ENABLE: 0);
	for(size_t i(0); i < platforms; ++i)
		for(size_t j(0); j < devices[i]; ++j)
		{
			queue[i][j] = clCreateCommandQueue(primary, device[i][j], qprop, &err);
			throw_on_error(err);
		}

	work::init();
}

ircd::cl::init::~init()
noexcept
{
	if(!linkage)
		return;

	const ctx::posix::enable_pthread enable_pthread;
	if(primary)
	{
		log::debug
		{
			log, "Shutting down OpenCL...",
		};

		work::fini();
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

	dlclose(linkage);
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

namespace ircd::cl
{
	static const size_t _deps_list_max {256};
	static thread_local cl_event _deps_list[_deps_list_max];

	static vector_view<cl_event> make_deps_default(cl::work *const &, const exec::opts &);
	static vector_view<cl_event> make_deps(cl::work *const &, const exec::opts &);
}

template<>
decltype(ircd::util::instance_list<ircd::cl::work>::allocator)
ircd::util::instance_list<ircd::cl::work>::allocator
{};

template<>
decltype(ircd::util::instance_list<ircd::cl::work>::list)
ircd::util::instance_list<ircd::cl::work>::list
{
    allocator
};

decltype(ircd::cl::exec::opts_default)
ircd::cl::exec::opts_default;

ircd::cl::exec::exec(const opts &opts)
try
{
	auto &q
	{
		queue[0][0]
	};

	const auto deps
	{
		make_deps(this, opts)
	};

	assert(!this->handle);
	call
	(
		clEnqueueBarrierWithWaitList,
		q,
		deps.size(),
		deps.size()? deps.data(): nullptr,
		reinterpret_cast<cl_event *>(&this->handle)
	);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Exec Barrier :%s",
		e.what(),
	};

	throw;
}

ircd::cl::exec::exec(kern &kern,
                     const kern::range &work,
                     const opts &opts)
try
{
	size_t dim(0);
	for(size_t i(0); i < work.global.size(); ++i)
		dim += work.global[i] > 0;

	auto &q
	{
		queue[0][0]
	};

	const auto deps
	{
		make_deps(this, opts)
	};

	assert(!this->handle);
	call
	(
		clEnqueueNDRangeKernel,
		q,
		cl_kernel(kern.handle),
		dim,
		work.offset.data(),
		work.global.data(),
		work.local.data(),
		deps.size(),
		deps.size()? deps.data(): nullptr,
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

ircd::cl::exec::exec(data &dst,
                     const data &src,
                     const opts &opts)
try
{
	auto &q
	{
		queue[0][0]
	};

	const auto deps
	{
		make_deps(this, opts)
	};

	const size_t size
	{
		opts.size?:
			std::min(dst.size(), src.size())
	};

	assert(!this->handle);
	call
	(
		clEnqueueCopyBuffer,
		q,
		cl_mem(src.handle),
		cl_mem(dst.handle),
		opts.offset[1],
		opts.offset[0],
		size,
		deps.size(),
		deps.size()? deps.data(): nullptr,
		reinterpret_cast<cl_event *>(&this->handle)
	);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Exec Copy :%s",
		e.what(),
	};

	throw;
}

ircd::cl::exec::exec(data &data,
                     const mutable_buffer &buf,
                     const opts &opts)
try
{
	auto &q
	{
		queue[0][0]
	};

	const auto deps
	{
		make_deps(this, opts)
	};

	assert(!this->handle);
	call
	(
		clEnqueueReadBuffer,
		q,
		cl_mem(data.handle),
		opts.blocking,
		opts.offset[0],
		ircd::size(buf),
		ircd::data(buf),
		deps.size(),
		deps.size()? deps.data(): nullptr,
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
                     const opts &opts)
try
{
	auto &q
	{
		queue[0][0]
	};

	const auto deps
	{
		make_deps(this, opts)
	};

	assert(!this->handle);
	call
	(
		clEnqueueWriteBuffer,
		q,
		cl_mem(data.handle),
		opts.blocking,
		opts.offset[0],
		ircd::size(buf),
		mutable_cast(ircd::data(buf)),
		deps.size(),
		deps.size()? deps.data(): nullptr,
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

ircd::cl::exec::exec(data &data,
                     const read_closure &closure,
                     const opts &opts)
try
{
	auto &q
	{
		queue[0][0]
	};

	const auto deps
	{
		make_deps(this, opts)
	};

	const auto size
	{
		opts.size?: data.size()
	};

	cl_map_flags flags {0};
	flags |= CL_MAP_READ;

	int err {CL_SUCCESS};
	assert(!this->handle);
	void *const ptr
	{
		clEnqueueMapBuffer
		(
			q,
			cl_mem(data.handle),
			opts.blocking,
			flags,
			opts.offset[0],
			size,
			deps.size(),
			deps.size()? deps.data(): nullptr,
			reinterpret_cast<cl_event *>(&this->handle),
			&err
		)
	};

	throw_on_error(err);
	assert(this->handle);
	assert(ptr);

	// Perform the unmapping on unwind. This is after the mapping event
	// completed and the closure was called below. The unmapping event will
	// replace the event handle for this exec instance until its actual dtor;
	// thus the lifetime of the exec we are constructing actually represents
	// the unmapping event.
	const unwind unmap{[this, &data, &q, &ptr]
	{
		assert(!this->handle);
		call
		(
			clEnqueueUnmapMemObject,
			q,
			cl_mem(data.handle),
			ptr,
			0, // deps
			nullptr, // depslist
			reinterpret_cast<cl_event *>(&this->handle)
		);
	}};

	// After the closure is called below, or throws, or if wait() throws,
	// we free the completed map event here to allow for the unmap event.
	const unwind rehandle{[this]
	{
		assert(this->handle);
		call(clReleaseEvent, cl_event(this->handle));
		this->handle = nullptr;
	}};

	// Wait for the mapping to complete before presenting the buffer.
	wait();
	closure(const_buffer
	{
		reinterpret_cast<const char *>(ptr), size
	});
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Exec Read Closure :%s",
		e.what(),
	};

	throw;
}

ircd::cl::exec::exec(data &data,
                     const write_closure &closure,
                     const opts &opts)
try
{
	auto &q
	{
		queue[0][0]
	};

	const auto deps
	{
		make_deps(this, opts)
	};

	const auto size
	{
		opts.size?: data.size()
	};

	cl_map_flags flags {0};
	flags |= CL_MAP_WRITE;
	flags |= opts.duplex? CL_MAP_READ: 0;

	int err {CL_SUCCESS};
	assert(!this->handle);
	void *const ptr
	{
		clEnqueueMapBuffer
		(
			q,
			cl_mem(data.handle),
			opts.blocking,
			flags,
			opts.offset[0],
			size,
			deps.size(),
			deps.size()? deps.data(): nullptr,
			reinterpret_cast<cl_event *>(&this->handle),
			&err
		)
	};

	throw_on_error(err);
	assert(this->handle);
	assert(ptr);

	const unwind unmap{[this, &data, &q, &ptr]
	{
		assert(!this->handle);
		call
		(
			clEnqueueUnmapMemObject,
			q,
			cl_mem(data.handle),
			ptr,
			0, // deps
			nullptr, // depslist
			reinterpret_cast<cl_event *>(&this->handle)
		);
	}};

	const unwind rehandle{[this]
	{
		assert(this->handle);
		call(clReleaseEvent, cl_event(this->handle));
		this->handle = nullptr;
	}};

	wait();
	closure(mutable_buffer
	{
		reinterpret_cast<char *>(ptr), size
	});
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Exec Write Closure :%s",
		e.what(),
	};

	throw;
}

ircd::vector_view<cl_event>
ircd::cl::make_deps(cl::work *const &work,
                    const exec::opts &opts)
{
	if(empty(opts.deps) && !opts.indep)
		return make_deps_default(work, opts);

	if(empty(opts.deps))
		return {};

	size_t ret(0);
	vector_view<cl_event> out(_deps_list);
	for(auto &exec : opts.deps)
		out.at(ret++) = cl_event(exec.handle);

	return vector_view<cl_event>
	{
		out, ret
	};
}

ircd::vector_view<cl_event>
ircd::cl::make_deps_default(cl::work *const &work,
                            const exec::opts &opts)
{
	size_t ret(0);
	vector_view<cl_event> out(_deps_list);
	for(auto it(rbegin(cl::work::list)); it != rend(cl::work::list); ++it)
	{
		cl::work *const &other{*it};
		if(other == work)
			continue;

		if(!other->handle)
			continue;

		if(other->context != ctx::current)
			continue;

		out.at(ret++) = cl_event(other->handle);
		break;
	}

	return vector_view<cl_event>
	{
		out, ret
	};
}

//
// kern
//

ircd::cl::kern::kern(code &code,
                     const string_view &name)
try
{
	int err {CL_SUCCESS};
	handle = clCreateKernel(cl_program(code.handle), name.c_str(), &err);
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
	call(clReleaseKernel, cl_kernel(handle));
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
	const auto &data_handle
	{
		cl_mem(data.handle)
	};

	call(clSetKernelArg, cl_kernel(handle), i, sizeof(cl_mem), &data_handle);
}

//
// code
//

ircd::cl::code::code(const string_view &src,
                     const string_view &build_opts)
:code
{
	vector_view<const string_view>(&src, 1),
	build_opts
}
{
}

ircd::cl::code::code(const vector_view<const string_view> &srcs,
                     const string_view &build_opts)
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

	if(!null(build_opts))
		build(build_opts);
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
	call(clReleaseProgram, cl_program(handle));
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
	static void build_handle_error_log_line(const string_view &line);
	static void build_handle_error(code &, const opencl_error &);
	static void build_handle(cl_program program, void *priv);
}

void
ircd::cl::code::build(const string_view &opts)
try
{
	const uint num_devices {0};
	const cl_device_id *const device_list {nullptr};
	call
	(
		clBuildProgram,
		cl_program(handle),
		num_devices,
		device_list,
		opts.c_str(),
		cl::build_handle,
		nullptr
	);
}
catch(const opencl_error &e)
{
	build_handle_error(*this, e);
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "code(%p) :Failed to build :%s",
		this,
		e.what(),
	};

	throw;
}

void
ircd::cl::build_handle(cl_program program,
                       void *const priv)
{
	ircd::always_assert(false);
}

void
ircd::cl::build_handle_error(code &code,
                             const opencl_error &e)
{
	const auto string_closure
	{
		[&code](const mutable_buffer &buf)
		{
			size_t len {0}; call
			(
				clGetProgramBuildInfo,
				cl_program(code.handle),
				device[0][0],
				CL_PROGRAM_BUILD_LOG,
				ircd::size(buf),
				ircd::data(buf),
				&len
			);

			return len;
		}
	};

	const auto error_message
	{
		ircd::string(8_KiB | SHRINK_TO_FIT, string_closure)
	};

	const auto lines
	{
		ircd::tokens(error_message, '\n', build_handle_error_log_line)
	};
}

void
ircd::cl::build_handle_error_log_line(const string_view &line)
{
	// note last line is just a CR
	if(line.size() <= 1)
		return;

	const auto &[loc, line_]    { split(line, ' ')   };
	const auto &[fac, msg]      { split(line_, ' ')  };
	const auto &[fname, pos]    { split(loc, ':')    };
	const auto &[row, col]      { split(pos, ':')    };

	const auto level
	{
		startswith(fac, "warning")?
			log::level::WARNING:

		startswith(fac, "error")?
			log::level::ERROR:

		log::level::ERROR
	};

	log::logf
	{
		log, level, "%s", line
	};
}

//
// data
//

ircd::cl::data::data(const size_t size_,
                     const mutable_buffer &buf,
                     const bool wonly)
{
	const auto ptr
	{
		ircd::size(buf)? ircd::data(buf): nullptr
	};

	const auto size
	{
		ircd::size(buf)?: size_
	};

	cl_mem_flags flags {0};
	flags |= CL_MEM_READ_WRITE;
	flags |= wonly? CL_MEM_WRITE_ONLY: 0;
	flags |= ircd::size(buf)? CL_MEM_COPY_HOST_PTR: 0;

	int err {CL_SUCCESS};
	handle = clCreateBuffer(primary, flags, size, ptr, &err);
	throw_on_error(err);
}

ircd::cl::data::data(const size_t size_,
                     const const_buffer &buf)
{
	const auto ptr
	{
		ircd::size(buf)? ircd::data(buf): nullptr
	};

	const auto size
	{
		ircd::size(buf)?: size_
	};

	cl_mem_flags flags {0};
	flags |= CL_MEM_READ_ONLY;
	flags |= ircd::size(buf)? CL_MEM_COPY_HOST_PTR: 0;

	int err {CL_SUCCESS};
	handle = clCreateBuffer(primary, flags, size, mutable_cast(ptr), &err);
	throw_on_error(err);
}

ircd::cl::data::data(const mutable_buffer &buf,
                     const bool wonly)
{
	cl_mem_flags flags {0};
	flags |= CL_MEM_USE_HOST_PTR;
	flags |= wonly? CL_MEM_WRITE_ONLY: CL_MEM_READ_WRITE;

	int err {CL_SUCCESS};
	handle = clCreateBuffer(primary, flags, ircd::size(buf), ircd::data(buf), &err);
	throw_on_error(err);
}

ircd::cl::data::data(const const_buffer &buf)
{
	cl_mem_flags flags {0};
	flags |= CL_MEM_USE_HOST_PTR;
	flags |= CL_MEM_READ_ONLY;

	int err {CL_SUCCESS};
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
	call(clReleaseMemObject, cl_mem(handle));
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

char *
ircd::cl::data::ptr()
const
{
	assert(handle);

	char buf[sizeof(void *)] {0};
	return info<char *>(clGetMemObjectInfo, cl_mem(mutable_cast(handle)), CL_MEM_SIZE, buf);
}

size_t
ircd::cl::data::size()
const
{
	assert(handle);

	char buf[sizeof(size_t)] {0};
	return info<size_t>(clGetMemObjectInfo, cl_mem(mutable_cast(handle)), CL_MEM_SIZE, buf);
}

uint
ircd::cl::data::flags()
const
{
	assert(handle);

	char buf[sizeof(uint)] {0};
	return info<uint>(clGetMemObjectInfo, cl_mem(mutable_cast(handle)), CL_MEM_FLAGS, buf);
}

//
// cl::work (event)
//

namespace ircd::cl
{
	struct completion;

	static void handle_event(cl_event, cl_int, void *) noexcept;
	static int handle_incomplete(work &, const int &, const int);
}

struct ircd::cl::completion
{
	cl_event event {nullptr};
	cl_int status {CL_COMPLETE};
	ctx::dock dock;
};

void
ircd::cl::work::init()
{

}

void
ircd::cl::work::fini()
noexcept
{
	cl::sync();
}

int
ircd::cl::handle_incomplete(work &work,
                            const int &status,
                            const int desired)
{
	completion c
	{
		cl_event(work.handle),
		status,
	};

	call
	(
		clSetEventCallback,
		cl_event(work.handle),
		desired,
		&handle_event,
		&c
	);

	c.dock.wait([&c, &desired]
	{
		return !c.event || c.status == desired;
	});

	return c.status;
}

void
ircd::cl::handle_event(cl_event event,
                       cl_int status,
                       void *const priv)
noexcept
{
	auto *const c
	{
		reinterpret_cast<completion *>(priv)
	};

	assert(event != nullptr);
	c->status = status;
	c->dock.notify_one();
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
	const unwind free{[this]
	{
		if(likely(handle))
			call(clReleaseEvent, cl_event(handle));
	}};

	wait();
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

bool
ircd::cl::work::wait()
try
{
	if(!handle)
		return false;

	char buf[4];
	int status
	{
		info<int>(clGetEventInfo, cl_event(handle), CL_EVENT_COMMAND_EXECUTION_STATUS, buf)
	};

	if(likely(status == CL_COMPLETE))
		return false;

	if(unlikely(status >= 0))
		status = handle_incomplete(*this, status, CL_COMPLETE);

	if(unlikely(status < 0))
		throw_on_error(status);

	assert(status == CL_COMPLETE);
	return true;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "work(%p) :%s",
		this,
		e.what(),
	};

	throw;
}

std::array<uint64_t, 4>
ircd::cl::work::profile()
const
{
	const auto handle
	{
		cl_event(this->handle)
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
			"(%d) :%s",
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
