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
	struct mmap;
	struct kern;
	struct code;
	struct data;
	struct work;

	extern log::log log;
	extern const info::versions version_api, version_abi;

	string_view reflect_error(const int code) noexcept;

	void flush();
	void sync();
}

/// cl_event wrapping
struct ircd::cl::work
{
	void *handle {nullptr};

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
	struct mmap;

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

/// cl_map wrapping
struct ircd::cl::data::mmap
:mutable_buffer
{
	cl::data *memory {nullptr};

  public:
	mmap(data &, const size_t size, const bool w = true, const bool wonly = false);
	mmap() = default;
	mmap(mmap &&) noexcept;
	mmap &operator=(const mmap &) = delete;
	mmap &operator=(mmap &&) noexcept;
	~mmap() noexcept;
};

/// cl_program wrapping
struct ircd::cl::code
{
	void *handle {nullptr};

  public:
	void build(const string_view &opts = {});

	code(const vector_view<const string_view> &srcs);
	code(const string_view &src);
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

struct ircd::cl::exec
:work
{
	exec(data &, const mutable_buffer &, const bool blocking = false);
	exec(data &, const const_buffer &, const bool blocking = false);
	exec(kern &, const kern::range &);
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
