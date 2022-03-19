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
#define HAVE_IRCD_CL_EXEC_H

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

	// Copy data written by the device to the GTT into our buffer.
	exec(data &, const std::memory_order, const opts & = opts_default);

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
	size_t size {-1UL};

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

inline
ircd::cl::exec::exec(kern &kern,
                     const opts &opts,
                     const kern::range &range)
:exec
{
	kern, range, opts
}
{}
