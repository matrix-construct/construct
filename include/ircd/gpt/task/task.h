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

#include "epic.h"
#include "tokens.h"
#include "gate.h"
#include "opts.h"
#include "ctrl.h"

#ifdef __cplusplus
/// Task Context
///
/// State for a task.
struct ircd::gpt::task
{
	enum status :char;

	/// Reference to the attached options.
	const gpt::opts *opts {nullptr};

	/// Reference to control pages.
	struct ircd_gpt_task *ctrl {nullptr};

	/// Current task status.
	enum status status {'\0'};

	task(const gpt::opts *       = nullptr,
	     struct ircd_gpt_task *  = nullptr);

	~task() noexcept;
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

static_assert(sizeof(struct ircd_gpt_task) == 4096);
static_assert(offsetof(struct ircd_gpt_task, token) == 2048);
static_assert(std::is_standard_layout<struct ircd_gpt_task>::value);
#endif
