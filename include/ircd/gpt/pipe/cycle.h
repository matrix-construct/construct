// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_GPT_PIPE_CYCLE_H

namespace ircd::gpt::pipe
{
	const gpt::ctrl &acquire(cycle &);
}

/// Perform one task cycle on the device. The cycle is an atomic unit of
/// generation producing one token.
///
/// Constructions of this object enqueue device commands to complete an
/// additional cycle of the task as provided by `ctrl` and `opts`.
///
/// Destructions of this object yield the ircd::ctx until those commands
/// are complete.
///
struct ircd::gpt::pipe::cycle
{
	struct profile;

	static constexpr size_t stages
	{
		4 + 3 + (12 * 2) + 4 + 2 + (12 * 2) + 1
	};

	pipe::desc &desc;
	uint tick;
	uint count;
	uint tokens;
	uint cached;
	uint frame;
	pipe::range range;
	std::array<cl::exec, stages> stage;

	cycle(gpt::samp &);
	~cycle() noexcept;
};
