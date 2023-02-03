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
#define HAVE_IRCD_CL_WORK_H

/// cl_event wrapping
struct ircd::cl::work
:instance_list<cl::work>
{
	struct prof;

	void *handle {nullptr};
	void *object {nullptr};
	ctx::ctx *context {ctx::current};
	uint64_t ts {ircd::cycles()};

	static void init(), fini() noexcept;

  public:
	explicit operator bool() const;

	int type() const;
	string_view name(const mutable_buffer &) const;

	void wait(const uint = 0);

	explicit work(void *const &handle); // note: RetainEvent()
	work();
	work(work &&) noexcept;
	work(const work &) = delete;
	work &operator=(work &&) noexcept;
	work &operator=(const work &) = delete;
	~work() noexcept;
};

template<>
decltype(ircd::cl::work::list)
ircd::instance_list<ircd::cl::work>::list;

/// Queue profiling convenience
struct ircd::cl::work::prof
:std::array<nanoseconds, 5>
{
	enum phase :int;

	prof(const cl::work &);
	prof() = default;
};

/// cl_profiling_info wrapper
enum ircd::cl::work::prof::phase
:int
{
	QUEUE,
	SUBMIT,
	START,
	END,
	COMPLETE,
	_NUM_
};

inline
ircd::cl::work::work(work &&other)
noexcept
:handle{std::move(other.handle)}
,object{std::move(other.object)}
,context{std::move(other.context)}
,ts{std::move(other.ts)}
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
	object = std::move(other.object);
	context = std::move(other.context);
	ts = std::move(other.ts);

	other.handle = nullptr;
	other.context = nullptr;

	return *this;
}

inline ircd::cl::work::operator
bool()
const
{
	return handle;
}
