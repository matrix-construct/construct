// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_GPT_TASK_EPIC_H

/// Epoch Precision Interrupt Controller
///
struct ircd_gpt_task_epic
{
	/// Accumulates the number of task cycles. The cycle counter is incremented
	/// by device software after each repetition of the kernel pipeline to
	/// produce one additional token.
	ulong cycle;

	/// Accumulates the epoch count for the task. The counter is incremented
	/// by one in device software before control returns back to the host.
	/// Several cycles may occur during each epoch.
	ulong epoch;

	/// Accumulates the training epoch count for the task. The counter is
	/// incremented by one in device software for each backward propagation.
	ulong step;

	/// Updated by the host with the value of the timestamp register as sampled
	/// immediately before each transfer of control to the device.
	ulong host_tsc;

	/// Accumulates time in microseconds elapsed for the task.
	ulong elapsed;
};
