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
#define HAVE_IRCD_GPT_TASK_H

/// Context to maintain state across calls.
///
struct ircd::gpt::task
{
	enum status :char;

	/// Reference to the attached options.
	const gpt::opts *opts {nullptr};

	/// Current task status.
	enum status status {'\0'};

	/// Accumulates the number of executions by the user. Each call to the
	/// interface is an execution.
	uint64_t epoch {0};

	/// Accumulates the number of tokens produced by the task. Several tokens
	/// may be produced each epoch.
	uint64_t produced {0};

	/// Accumulates the number tokens witnessed by the task. The number of
	/// tokens in the context for each produced token is counted as witnessed.
	uint64_t witnessed {0};

	/// Accumulates the number of CPU reference cycles consumed by the task.
	/// This counter does not reflect time when the task is queued or waiting
	/// or offloaded to a co-processor/accelerator.
	uint64_t cycles {0};

	/// Accumulates the total time in milliseconds over all executions of the
	/// task. This counter reflects total wall-clock time of all phases of
	/// the execution.
	milliseconds time {0ms};
};

/// The current status of a task is indicated with intelligible characters
enum ircd::gpt::task::status
:char
{
	QUEUED    = 'Q',  ///< Queued for execution.
	RUNNING   = 'R',  ///< Currently being executed.
	ACCEPT    = 'A',  ///< Execution completed successfully.
	ERROR     = 'E',  ///< Execution did not complete successfully.
};
