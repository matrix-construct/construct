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
#define HAVE_IRCD_GPT_TASK_TOKENS_H

/// Token Context Buffer (Control Block)
///
struct ircd_gpt_task_tokens
{
	/// Token ring head. Tokens in the ring extend behind the head for
	/// `tokens`. The `head` value is automatically modulated by device
	/// software to wrap around the ring.
	uint head;

	/// Token counter. The counter indicates the number of valid tokens in
	/// the context buffer. This value must not exceed the buffer size.
	uint count;

	/// Accumulates the number of tokens produced by the task. Several tokens
	/// may be produced each epoch, but currently only one token is produced
	/// each cycle.
	ulong produced;

	/// Accumulates the number tokens witnessed by the task. The number of
	/// tokens in the context for each cycle is counted as witnessed.
	ulong witnessed;
};
