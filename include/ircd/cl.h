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
	struct init;
	struct exec;
	struct kern;
	struct code;
	struct data;
	struct work;

	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, opencl_error)

	using read_closure = std::function<void (const_buffer)>;
	using write_closure = std::function<void (mutable_buffer)>;

	extern const info::versions version_api;
	extern info::versions version_abi;
	extern conf::item<milliseconds> nice_rate;
	extern conf::item<std::string> envs[];
	extern conf::item<bool> profile_queue;
	extern conf::item<bool> enable;
	extern log::log log;

	string_view reflect_error(const int code) noexcept;

	void log_dev_info(const uint platform_id, const uint device_id);
	void log_dev_info(const uint platform_id);
	void log_dev_info();

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

	void wait(const uint = 0);

	explicit work(void *const &handle); // note: RetainEvent()
	work() noexcept;
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
	uint flags() const;
	size_t size() const;
	char *ptr() const; // host only

	data(const size_t, const mutable_buffer &, const bool wonly = false); // device rw
	data(const size_t, const const_buffer &); // device ro
	data(const mutable_buffer &, const bool wonly = false); // host rw
	data(const const_buffer &); // host ro
	data(data &, const pair<size_t, off_t> &); // subbuffer
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

	explicit code(const vector_view<const const_buffer> &bins, const string_view &opts = {});
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
	std::array<size_t, 3> compile_group_size(void *dev = nullptr) const;
	size_t preferred_group_size_multiple(void *dev = nullptr) const;
	size_t group_size(void *dev = nullptr) const;
	size_t local_mem_size(void *dev = nullptr) const;
	size_t stack_mem_size(void *dev = nullptr) const;

	void arg(const int, data &);
	void arg(const int, const const_buffer &);
	template<class T> void arg(const int, const T &);

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
	global { 0, 0, 0, 0, 0 },
	local  { 0, 0, 0, 0, 0 },
	offset { 0, 0, 0, 0, 0 };
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

	// View data written by the device to the GTT (synchronous closure).
	exec(data &, const pair<size_t, off_t> &, const read_closure &, const opts & = opts_default);

	// View buffer in the GTT which the device will read (synchronous closure).
	exec(data &, const pair<size_t, off_t> &, const write_closure &, const opts & = opts_default);

	// Copy data from the buffer to the GTT for use by the device.
	exec(data &, const const_buffer &, const opts & = opts_default);

	// Copy data written by the device to the GTT into our buffer.
	exec(data &, const mutable_buffer &, const opts & = opts_default);

	// Copy data directly between buffers.
	exec(data &, const data &, const opts & = opts_default);

	// Execute a kernel on a range.
	exec(kern &, const kern::range &, const opts & = opts_default);

	// Execute a kernel on a range.
	exec(kern &, const opts &, const kern::range &);

	// Execute a barrier.
	exec(const opts &);

	// No-op
	exec() = default;
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

	/// For operations which have a size; otherwise ignored, or serves as
	/// sentinel for automatic size.
	size_t size {0};

	/// For operations which have an offset (or two); otherwise ignored.
	off_t offset[2] {0};

	/// Tune the intensity of the execution. For headless deployments the
	/// maximum intensity is advised. Lesser values are more intense. The
	/// default of -1 is the maximum. The value of zero yields the ircd::ctx
	/// after submission, but does not otherwise decrease the intensity.
	int nice {-1};

	/// Starts a new dependency chain; allowing empty deps without implicit
	/// dependency on the last work item constructed on the ircd::ctx.
	bool indep {false};

	/// For operations which plan to both read and write to the GTT, set to
	/// true and execute the write_closure; otherwise ignored. Can be used
	/// to de-optimize the write_closure, which is unidirectional by default.
	bool duplex {false};

	/// For operations that have an optional blocking behavior; otherwise
	/// ignored. Note that this is a thread-level blocking mechanism and
	/// does not yield the ircd::ctx; for testing/special use only.
	bool blocking {false};

	/// Perform a flush of the queue directly after submit.
	bool flush {false};

	/// Perform a sync of the queue directly after submit; this will block in
	/// the ctor; all work will be complete at full construction.
	bool sync {false};
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
	uint i(0);
	(this->arg(i++, a), ...);
}

template<class T>
inline void
ircd::cl::kern::arg(const int pos,
                    const T &val)
{
	static_assert(!std::is_same<T, cl::data>());
	arg(pos, const_buffer
	{
		reinterpret_cast<const char *>(&val), sizeof(T)
	});
}

inline
ircd::cl::exec::exec(kern &kern,
                     const opts &opts,
                     const kern::range &range)
:exec
{
	kern, range, opts
}
{}
