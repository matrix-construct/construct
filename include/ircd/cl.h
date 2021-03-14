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
	struct exec;
	struct kern;
	struct code;
	struct data;
	struct work;

	extern const info::versions version_api;
	extern info::versions version_abi;
	extern conf::item<bool> enable;
	extern log::log log;

	string_view reflect_error(const int code) noexcept;

	void flush();
	void sync();
}

/// cl_event wrapping
struct ircd::cl::work
:instance_list<cl::work>
{
	void *handle {nullptr};
	ctx::ctx *context {ctx::current};

	static void init(), fini() noexcept;

  public:
	std::array<uint64_t, 4> profile() const;

	work(void *const &handle); // note: RetainEvent()
	work() = default;
	work(work &&) noexcept;
	work(const work &) = delete;
	work &operator=(work &&) noexcept;
	work &operator=(const work &) = delete;
	~work() noexcept;
};

/// cl_mem wrapping
struct ircd::cl::data
{
	void *handle {nullptr};

  public:
	data(const size_t, const bool w = false, const bool wonly = false);
	data(const mutable_buffer &, const bool wonly = false); // host
	data(const const_buffer &); // host
	data(const data &) = delete;
	data() = default;
	data(data &&) noexcept;
	data &operator=(const data &) = delete;
	data &operator=(data &&) noexcept;
	~data() noexcept;
};

/// cl_program wrapping
struct ircd::cl::code
{
	void *handle {nullptr};

  public:
	void build(const string_view &opts = {});

	code(const vector_view<const string_view> &srcs, const string_view &opts = {});
	code(const string_view &src, const string_view &opts = {});
	code() = default;
	code(code &&) noexcept;
	code &operator=(const code &) = delete;
	code &operator=(code &&) noexcept;
	~code() noexcept;
};

/// cl_kernel wrapping
struct ircd::cl::kern
{
	struct range;

	void *handle {nullptr};

  public:
	void arg(const int, data &);

	template<class... argv> kern(code &, const string_view &name, argv&&...);
	kern(code &, const string_view &name);
	kern() = default;
	kern(kern &&) noexcept;
	kern &operator=(const kern &) = delete;
	kern &operator=(kern &&) noexcept;
	~kern() noexcept;
};

/// NDRangeKernel dimension range selector
struct ircd::cl::kern::range
{
	std::array<size_t, 5>
	offset { 0, 0, 0, 0, 0 },
	global { 0, 0, 0, 0, 0 },
	local  { 0, 0, 0, 0, 0 };
};

/// Construction enqueues the task; destruction waits for completion.
///
/// clEnqueue* construction with resulting cl_event wrapping. Instances
/// represent the full lifecycle of work creation, submission and completion.
///
/// Our interface is tied directly to ircd::ctx for intuitive control flow and
/// interaction with the device. By default, all constructions are dependent
/// on the last construction made on the same ircd::ctx, providing sequential
/// consistency for each ircd::ctx, and independence between different ctxs.
/// Each instance destructs only when complete, otherwise the ircd::ctx will
/// block in the destructor.
struct ircd::cl::exec
:work
{
	struct opts;

	static const opts opts_default;

	// Read data from the device into buffer.
	exec(data &, const mutable_buffer &, const opts & = opts_default);

	// Write data to the device from buffer.
	exec(data &, const const_buffer &, const opts & = opts_default);

	// Execute a kernel on a range.
	exec(kern &, const kern::range &, const opts & = opts_default);

	// Execute a barrier.
	exec(const opts &);
};

/// Options for an exec.
struct ircd::cl::exec::opts
{
	/// Specify a list of dependencies. When provided, this list overrides the
	/// default sequential behavior; thus can be used to start new dependency
	/// chains for some task concurrency on the same ircd::ctx. Providing a
	/// single reference to the last exec on the same stack is equivalent to
	/// the default.
	vector_view<cl::exec> deps;

	/// For operations that have an optional blocking behavior;
	/// otherwise ignored.
	bool blocking {false};
};

struct ircd::cl::init
{
	init();
	~init() noexcept;
};

#ifndef IRCD_USE_OPENCL
inline ircd::cl::init::init() {}
inline ircd::cl::init::~init() noexcept {}
#endif

inline
ircd::cl::work::work(work &&other)
noexcept
:handle{std::move(other.handle)}
,context{std::move(other.context)}
{
	other.handle = nullptr;
	other.context = nullptr;
}

inline ircd::cl::work &
ircd::cl::work::operator=(work &&other)
noexcept
{
	this->~work();
	handle = std::move(other.handle);
	context = std::move(other.context);
	other.handle = nullptr;
	other.context = nullptr;
	return *this;
}

template<class... argv>
inline
ircd::cl::kern::kern(code &c,
                     const string_view &name,
                     argv&&... a)
:kern{c, name}
{
	constexpr uint argc
	{
		sizeof...(a)
	};

	data *const datas[argc]
	{
		std::addressof(a)...
	};

	for(uint i(0); i < argc; ++i)
		this->arg(i, *datas[i]);
}
